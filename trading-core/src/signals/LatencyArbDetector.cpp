#include "LatencyArbDetector.h"
#include "../model/FairValueModel.h"
#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <cmath>
#include <algorithm>

namespace trading {

LatencyArbDetector::LatencyArbDetector(StateStore& state_store, 
                                       std::vector<MarketInfo> active_markets,
                                       double min_edge_threshold,
                                       double min_seconds_remaining,
                                       double cooldown_seconds,
                                       double lag_window_seconds,
                                       std::string asset,
                                       PriceResolver price_resolver)
    : state_store_(state_store),
      active_markets_(std::move(active_markets)),
      min_edge_threshold_(min_edge_threshold),
      min_seconds_remaining_(min_seconds_remaining),
      cooldown_seconds_(cooldown_seconds),
      lag_window_seconds_(lag_window_seconds),
      asset_(std::move(asset)),
      price_resolver_(std::move(price_resolver))
{
    asset_configs_["btc"] = {150.0, 20.0, 5.0};
    asset_configs_["eth"] = {10.0, 1.5, 0.53};
    asset_configs_["sol"] = {1.5, 0.2, 0.05};

    spdlog::info("LatencyArbDetector initialized | Asset: {} | Markets: {} | MinEdge: {:.3f} | Lag: {:.1f}s",
                 asset_, active_markets_.size(), min_edge_threshold_, lag_window_seconds_);
}

double LatencyArbDetector::compute_fair_value_5m(double price_now, double price_to_beat, 
                                               double seconds_remaining, const std::string& direction,
                                               const AssetConfig& cfg) {
    double t_frac = std::max(0.01, seconds_remaining / window_seconds_);
    double scale = cfg.base_scale * std::sqrt(t_frac) + cfg.min_scale;
    
    double distance = (price_now - price_to_beat) / scale;
    double p_up = 1.0 / (1.0 + std::exp(-distance));
    
    double fair_value = (direction == "UP") ? p_up : (1.0 - p_up);
    return std::max(0.01, std::min(0.99, fair_value));
}

std::optional<LatencyArbSignal> LatencyArbDetector::evaluate(double current_time_ms) {
    evaluations_++;
    std::optional<LatencyArbSignal> best_signal;

    auto price_now_tick = state_store_.get_latest_price(asset_);
    if (price_now_tick.price <= 0) return std::nullopt;
    double price_now = price_now_tick.price;

    auto price_lag_opt = state_store_.get_price_at(asset_, lag_window_seconds_);
    if (!price_lag_opt) return std::nullopt;
    double price_lag = *price_lag_opt;

    double move = price_now - price_lag;
    if (move == 0) return std::nullopt;

    const auto& cfg = asset_configs_[asset_];
    if (std::abs(move) < cfg.min_price_move) return std::nullopt;

    // Volatility Filter: Skip if price is moving too fast (>0.5% in 1s)
    double now_s = current_time_ms / 1000.0;
    if (last_price_time_ > 0) {
        double dt = now_s - last_price_time_;
        if (dt > 0 && dt < 2.0) {
            double change = std::abs(price_now - last_price_now_) / last_price_now_;
            if (change / dt > 0.005) {
                return std::nullopt; 
            }
        }
    }
    last_price_now_ = price_now;
    last_price_time_ = now_s;

    std::string direction = (move > 0) ? "UP" : "DOWN";

    for (const auto& market : active_markets_) {
        if (market.asset != asset_) continue;

        // Check cooldown
        if (last_signal_time_.contains(asset_)) {
            if ((current_time_ms - last_signal_time_[asset_]) / 1000.0 < cooldown_seconds_) {
                continue;
            }
        }

        double seconds_remaining = market.end_date_ts - (current_time_ms / 1000.0);
        if (seconds_remaining < min_seconds_remaining_) continue;
        if (seconds_remaining > window_seconds_ - 1.0) continue;

        // Resolve ACTUAL price-to-beat from history
        double window_elapsed = window_seconds_ - seconds_remaining;
        auto ptb_opt = state_store_.get_price_at(asset_, window_elapsed);
        if (!ptb_opt) {
            spdlog::debug("LatencyArbDetector: PTB unavailable for {} (elapsed: {:.0f}s)", asset_, window_elapsed);
            continue;
        }
        double price_to_beat = *ptb_opt;

        // Calculate Fair Value using time-aware sigmoid
        double fair_value = compute_fair_value_5m(price_now, price_to_beat, seconds_remaining, direction, cfg);

        // Strict Filter: Conviction Strength
        if (std::abs(fair_value - 0.5) < min_fair_value_strength_) continue;

        // --- Price Resolution: WS cache then REST fallback ---
        // Mirrors Python's get_market_price(): try WS cache first (zero-latency),
        // fall back to REST /price endpoint only when cache misses.
        // This is the critical fix vs. the old WS-only path.
        std::string target_token = (direction == "UP") ? market.yes_token_id : market.no_token_id;
        double pm_price = 0.0;
        bool ws_hit = false;

        auto ws_price_opt = state_store_.get_token_price(target_token);
        if (ws_price_opt && ws_price_opt->price > 0.0 && ws_price_opt->price < 1.0) {
            pm_price = ws_price_opt->price;
            ws_hit = true;
        } else if (price_resolver_) {
            // WS cache miss — fall back to REST (blocking, ~200ms)
            auto rest_price = price_resolver_(target_token, "BUY");
            if (!rest_price) {
                spdlog::debug("LatencyArbDetector: WS miss + REST miss for token {}...",
                              target_token.substr(0, 16));
                continue;
            }
            pm_price = *rest_price;
            spdlog::debug("LatencyArbDetector: WS miss, REST fallback price {:.4f} for {}...",
                          pm_price, target_token.substr(0, 16));
        } else {
            // No REST resolver configured — skip (old behaviour)
            continue;
        }

        if (pm_price <= 0.0 || pm_price >= 1.0) continue;

        // Strict Filter: Entry Zone
        if (pm_price < min_entry_price_ || pm_price > max_entry_price_) continue;

        double edge = fair_value - pm_price;
        
        // Time-Decay Edge Filter: Increase required edge as expiry nears
        double dynamic_min_edge = min_edge_threshold_;
        if (seconds_remaining < 120.0) dynamic_min_edge *= 1.5;
        if (seconds_remaining < 60.0)  dynamic_min_edge *= 2.0;
        
        if (edge > dynamic_min_edge) {
            LatencyArbSignal signal{
                .market = market,
                .asset = asset_,
                .token_id = target_token,
                .side = "BUY",
                .polymarket_price = pm_price,
                .binance_price = price_now,
                .fair_value = fair_value,
                .edge = edge,
                .seconds_remaining = seconds_remaining,
                .timestamp = current_time_ms
            };
            if (!best_signal || signal.edge > best_signal->edge) {
                best_signal = signal;
            }
        }
    }

    if (best_signal) {
        last_signal_time_[asset_] = current_time_ms;
        signals_generated_++;
        std::string dir = (best_signal->token_id == best_signal->market.yes_token_id) ? "YES" : "NO";
        spdlog::info("LATENCY-ARB DETECTED [#{}] | {} {} | Edge: {:.3f} (Fair: {:.3f}, PM: {:.3f}) | Delta: {:+.2f}",
                     signals_generated_, best_signal->asset, dir, 
                     best_signal->edge, best_signal->fair_value, best_signal->polymarket_price, move);
    }

    return best_signal;
}

void LatencyArbDetector::reset_cooldown(const std::string& asset, double current_time_ms) {
    last_signal_time_[asset] = current_time_ms;
}

} // namespace trading
