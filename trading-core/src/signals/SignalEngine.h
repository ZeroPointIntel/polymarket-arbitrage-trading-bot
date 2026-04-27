#pragma once
#include "LatencyArbDetector.h"
#include "DumpHedgeDetector.h"
#include "../risk/RiskManager.h"
#include "../exec/OrderRouter.h"
#include "../state/StateStore.h"
#include <memory>
#include <string>

namespace trading {

class SignalEngine {
public:
    SignalEngine(StateStore& state_store, 
                 std::shared_ptr<risk::RiskManager> risk_manager,
                 std::shared_ptr<exec::OrderRouter> order_router,
                 std::vector<MarketInfo> active_markets);

    // Call this periodically or on feed updates
    void tick(double current_time_ms);

private:
    void handle_latency_signal(const LatencyArbSignal& sig);
    void handle_dump_hedge_signal(const DumpHedgeSignal& sig);

    StateStore& state_store_;
    std::shared_ptr<risk::RiskManager> risk_manager_;
    std::shared_ptr<exec::OrderRouter> order_router_;

    LatencyArbDetector latency_detector_;
    DumpHedgeDetector dump_hedge_detector_;

    // Default sizing
    double trade_size_usd_ = 50.0;
};

} // namespace trading
