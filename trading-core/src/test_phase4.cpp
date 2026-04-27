#include "state/StateStore.h"
#include "signals/SignalEngine.h"
#include "risk/RiskManager.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <iostream>

using namespace trading;

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting Phase 4 Test");

    StateStore state_store;

    // Dummy risk manager config
    auto risk_manager = std::make_shared<risk::RiskManager>(
        1000.0, // starting_balance
        0.5,    // max_position_fraction
        0.20,   // daily_loss_limit
        0.40,   // total_drawdown_kill
        5       // max_concurrent_positions
    );

    MarketInfo btc_market{
        .condition_id = "cond_1",
        .question = "Bitcoin Up or Down - 5 Minutes?",
        .asset = "btc",
        .yes_token_id = "token_yes_1",
        .no_token_id = "token_no_1",
        .strike = 65000.0,
        .end_date_ts = 10000.0 + 300.0 // 5 minutes from 10000
    };

    std::vector<MarketInfo> active_markets = { btc_market };

    // We pass nullptr for order_router as we just want to test signal generation
    SignalEngine engine(state_store, risk_manager, nullptr, active_markets);

    double current_time_ms = 10000.0 * 1000.0;

    spdlog::info("--- TEST 1: No Signals ---");
    state_store.update_btc_price({65000.0, current_time_ms, 1.0, current_time_ms});
    
    state_store.update_token_price("token_yes_1", {0.50, "BUY", current_time_ms});
    state_store.update_token_price("token_no_1", {0.50, "BUY", current_time_ms});
    
    engine.tick(current_time_ms);
    // Expecting no signals: Fair Value = 0.5 (btc == strike), Edge = 0. 
    // Dump Hedge Sum = 1.0 > 0.95

    spdlog::info("--- TEST 2: Latency Arb Signal ---");
    // BTC spikes to 66000. Fair Value goes to ~1.0
    state_store.update_btc_price({66000.0, current_time_ms + 1000, 1.0, current_time_ms + 1000});
    engine.tick(current_time_ms + 1000);
    // Expecting Latency Arb BUY YES signal because PM price is still 0.50 but FV is 1.0

    spdlog::info("--- TEST 3: Dump Hedge Signal ---");
    // BTC goes back to 65000, PM orderbook gets out of whack
    state_store.update_btc_price({65000.0, current_time_ms + 10000, 1.0, current_time_ms + 10000});
    
    // Someone dumps tokens, asks become 0.45 and 0.45
    state_store.update_token_price("token_yes_1", {0.45, "BUY", current_time_ms + 10000});
    state_store.update_token_price("token_no_1", {0.45, "BUY", current_time_ms + 10000});
    
    engine.tick(current_time_ms + 10000);
    // Expecting Dump Hedge Signal because 0.45 + 0.45 = 0.90 < 0.95 sum_target

    spdlog::info("Phase 4 Test Completed!");
    return 0;
}
