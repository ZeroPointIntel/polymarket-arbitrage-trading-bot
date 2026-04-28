#include "LatencyArbDetector.h"
#include "../model/FairValueModel.h"
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace trading {

LatencyArbDetector::LatencyArbDetector(StateStore& state_store, 
                                       std::vector<MarketInfo> active_markets,
                                       double min_edge_threshold,
                                       double min_seconds_remaining,
                                       double cooldown_seconds,
                                       std::string asset)
    : state_store_(state_store),
      active_markets_(std::move(active_markets)),
      min_edge_threshold_(min_edge_threshold),
      min_seconds_remaining_(min_seconds_remaining),
      cooldown_seconds_(cooldown_seconds),
      asset_(std::move(asset))
{
    std::string msg = fmt::format("LatencyArbDetector initialized | Asset: {} | Markets: {} | MinEdge: {:.3f}",
                                  asset_, active_markets_.size(), min_edge_threshold_);
    spdlog::log(spdlog::level::info, msg);
}

std::optional<LatencyArbSignal> LatencyArbDetector::evaluate(double current_time_ms) {
    evaluations_++;
    std::optional<LatencyArbSignal> best_signal;

    if (asset_ != "btc") return std::nullopt;

    auto btc_tick = state_store_.get_latest_btc_price();
    if (!btc_tick) return std::nullopt;

    double btc_price = btc_tick->price;

    for (const auto& market : active_markets_) {
        // Only evaluate LA for the configured asset (e.g. btc)
        if (market.asset != asset_) continue;

        // Check cooldown
        if (last_signal_time_.contains(market.asset)) {
            if ((current_time_ms - last_signal_time_[market.asset]) / 1000.0 < cooldown_seconds_) {
                continue;
            }
        }

        double seconds_remaining = market.end_date_ts - (current_time_ms / 1000.0);
        if (seconds_remaining < min_seconds_remaining_) {
            continue;
        }

        // Calculate Fair Value
        double fair_value_up = model::FairValueModel::compute_fair_value_5m(
            btc_price, market.strike, seconds_remaining, "UP", 150.0, 20.0);
        double fair_value_down = model::FairValueModel::compute_fair_value_5m(
            btc_price, market.strike, seconds_remaining, "DOWN", 150.0, 20.0);

        // Fetch Polymarket best ask for YES (UP) and NO (DOWN)
        auto yes_price_opt = state_store_.get_token_price(market.yes_token_id);
        auto no_price_opt = state_store_.get_token_price(market.no_token_id);

        double yes_price = (yes_price_opt && yes_price_opt->side == "BUY") ? yes_price_opt->price : 0.0;
        double no_price = (no_price_opt && no_price_opt->side == "BUY") ? no_price_opt->price : 0.0;

        // Check edge for YES
        if (yes_price > 0.0) {
            double edge_yes = fair_value_up - yes_price;
            if (edge_yes > min_edge_threshold_) {
                LatencyArbSignal signal{
                    .market = market,
                    .asset = market.asset,
                    .token_id = market.yes_token_id,
                    .side = "BUY",
                    .polymarket_price = yes_price,
                    .binance_price = btc_price,
                    .fair_value = fair_value_up,
                    .edge = edge_yes,
                    .seconds_remaining = seconds_remaining,
                    .timestamp = current_time_ms
                };
                if (!best_signal || signal.edge > best_signal->edge) {
                    best_signal = signal;
                }
            }
        }

        // Check edge for NO
        if (no_price > 0.0) {
            double edge_no = fair_value_down - no_price;
            if (edge_no > min_edge_threshold_) {
                LatencyArbSignal signal{
                    .market = market,
                    .asset = market.asset,
                    .token_id = market.no_token_id,
                    .side = "BUY",
                    .polymarket_price = no_price,
                    .binance_price = btc_price,
                    .fair_value = fair_value_down,
                    .edge = edge_no,
                    .seconds_remaining = seconds_remaining,
                    .timestamp = current_time_ms
                };
                if (!best_signal || signal.edge > best_signal->edge) {
                    best_signal = signal;
                }
            }
        }
    }

    if (best_signal) {
        last_signal_time_[best_signal->asset] = current_time_ms;
        signals_generated_++;
        std::string direction = (best_signal->token_id == best_signal->market.yes_token_id) ? "YES" : "NO";
        std::string msg = fmt::format("LATENCY-ARB DETECTED [#{}] | {} {} | Edge: {:.3f} (Fair: {:.3f}, PM: {:.3f})",
                                      signals_generated_, best_signal->asset, direction, 
                                      best_signal->edge, best_signal->fair_value, best_signal->polymarket_price);
        spdlog::log(spdlog::level::info, msg);
    }

    return best_signal;
}

void LatencyArbDetector::reset_cooldown(const std::string& asset, double current_time_ms) {
    last_signal_time_[asset] = current_time_ms;
}

} // namespace trading
