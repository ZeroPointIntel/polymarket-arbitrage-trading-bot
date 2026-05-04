#include "SignalEngine.h"
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace trading {

SignalEngine::SignalEngine(StateStore& state_store, 
                           std::shared_ptr<risk::RiskManager> risk_manager,
                           std::shared_ptr<exec::OrderRouter> order_router,
                           std::vector<MarketInfo> active_markets)
    : state_store_(state_store),
      risk_manager_(std::move(risk_manager)),
      order_router_(std::move(order_router)),
      latency_detector_(state_store_, active_markets),
      dump_hedge_detector_(state_store_, active_markets),
      kelly_sizer_(0.5, 0.08, 1.0, 0.0, true, 0.1) // Adaptive enabled by default for HFT
{
    spdlog::info("SignalEngine initialized with {} active markets", active_markets.size());
}

void SignalEngine::tick(double current_time_ms) {
    // 1. Check Latency Arb
    auto latency_sig = latency_detector_.evaluate(current_time_ms);
    if (latency_sig) {
        handle_latency_signal(*latency_sig);
    }

    // 2. Check Dump Hedge
    auto dump_hedge_sig = dump_hedge_detector_.evaluate(current_time_ms);
    if (dump_hedge_sig) {
        handle_dump_hedge_signal(*dump_hedge_sig);
    }
}

void SignalEngine::handle_latency_signal(const LatencyArbSignal& sig) {
    double bankroll = risk_manager_->get_current_balance();
    double win_rate = risk_manager_->get_win_rate();
    
    auto kelly = kelly_sizer_.calculate(bankroll, sig.fair_value, sig.polymarket_price, win_rate);
    
    if (!kelly) {
        spdlog::debug("SignalEngine: LA signal for {} skipped due to Kelly sizing (Fair: {:.3f}, Price: {:.3f})", 
                      sig.token_id, sig.fair_value, sig.polymarket_price);
        return;
    }

    std::string order_id = fmt::format("LA-{}-{}", sig.token_id, static_cast<long long>(sig.timestamp));
    double num_shares = kelly->position_size_usdc / sig.polymarket_price;
    double cost_usdc = kelly->position_size_usdc;

    auto [can_open, reason] = risk_manager_->can_open_position(cost_usdc);

    if (can_open) {
        spdlog::info("SignalEngine: Approved LA order {} | {} | Cost: ${:.2f}", 
                     order_id, kelly->to_string(), cost_usdc);
        
        uint8_t side_val = (sig.side == "BUY") ? 0 : 1;
        
        if (order_router_) {
            order_router_->submit_order(sig.token_id, sig.polymarket_price, num_shares, side_val);
        }

        // Mark position as opened in risk manager
        risk::Position pos{
            .order_id = order_id,
            .token_id = sig.token_id,
            .market_question = sig.market.question,
            .side = sig.side,
            .entry_price = sig.polymarket_price,
            .size_shares = num_shares,
            .cost_usdc = cost_usdc,
            .opened_at = sig.timestamp,
            .asset = sig.asset,
            .direction = (sig.token_id == sig.market.yes_token_id) ? "YES" : "NO",
            .condition_id = sig.market.condition_id,
            .closed_at = std::nullopt,
            .exit_price = std::nullopt,
            .pnl_usdc = std::nullopt,
            .paper_mode = true
        };
        risk_manager_->register_trade_open(pos);
        
        latency_detector_.reset_cooldown(sig.asset, sig.timestamp);
    } else {
        spdlog::warn("SignalEngine: LA order {} rejected by RiskManager: {}", order_id, reason);
    }
}

void SignalEngine::handle_dump_hedge_signal(const DumpHedgeSignal& sig) {
    double bankroll = risk_manager_->get_current_balance();
    double win_rate = risk_manager_->get_win_rate();
    
    // For DH, fair value is 1.0 (combined price should be < 1.0)
    auto kelly = kelly_sizer_.calculate(bankroll, 0.99, sig.combined_price, win_rate);
    
    if (!kelly) {
        spdlog::debug("SignalEngine: DH signal for {} skipped due to Kelly sizing", sig.asset);
        return;
    }

    std::string order_id_yes = fmt::format("DH-YES-{}-{}", sig.yes_token_id, static_cast<long long>(sig.timestamp));
    std::string order_id_no = fmt::format("DH-NO-{}-{}", sig.no_token_id, static_cast<long long>(sig.timestamp));
    
    double num_shares = kelly->position_size_usdc / sig.combined_price; 
    double combined_cost_usdc = kelly->position_size_usdc;

    auto [can_open, reason] = risk_manager_->can_open_dh_position(combined_cost_usdc);

    if (can_open) {
        spdlog::info("SignalEngine: Approved DH pair {} | {} | Cost: ${:.2f}", 
                     sig.asset, kelly->to_string(), combined_cost_usdc);
        
        if (order_router_) {
            order_router_->submit_order(sig.yes_token_id, sig.yes_price, num_shares, 0);
            order_router_->submit_order(sig.no_token_id, sig.no_price, num_shares, 0);
        }

        risk::DumpHedgePosition dh_pos{
            .dh_id = fmt::format("DH-{}-{}", sig.asset, static_cast<long long>(sig.timestamp)),
            .yes_order_id = order_id_yes,
            .no_order_id = order_id_no,
            .yes_token_id = sig.yes_token_id,
            .no_token_id = sig.no_token_id,
            .market_question = sig.market.question,
            .asset = sig.asset,
            .yes_entry_price = sig.yes_price,
            .no_entry_price = sig.no_price,
            .combined_entry_price = sig.combined_price,
            .size_shares = num_shares,
            .combined_cost_usdc = combined_cost_usdc,
            .locked_profit_usdc = num_shares * sig.discount,
            .opened_at = sig.timestamp,
            .paper_mode = true,
            .closed_at = std::nullopt,
            .yes_exit_price = std::nullopt,
            .no_exit_price = std::nullopt,
            .pnl_usdc = std::nullopt,
            .exit_reason = ""
        };

        risk_manager_->register_dh_open(dh_pos);
        dump_hedge_detector_.reset_cooldown(sig.asset, sig.timestamp);
    } else {
        spdlog::warn("SignalEngine: DH order pair rejected by RiskManager: {}", reason);
    }
}

} // namespace trading
