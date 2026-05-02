#pragma once
#include "Signal.h"
#include "../state/StateStore.h"
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

namespace trading {

class LatencyArbDetector {
public:
    LatencyArbDetector(StateStore& state_store, 
                       std::vector<MarketInfo> active_markets,
                       double min_edge_threshold = 0.04,
                       double min_seconds_remaining = 60.0,
                       double cooldown_seconds = 5.0,
                       std::string asset = "btc");

    std::optional<LatencyArbSignal> evaluate(double current_time_ms);
    void reset_cooldown(const std::string& asset, double current_time_ms);

private:
    StateStore& state_store_;
    std::vector<MarketInfo> active_markets_;
    double min_edge_threshold_;
    double min_seconds_remaining_;
    double cooldown_seconds_;
    std::string asset_;

    std::unordered_map<std::string, double> last_signal_time_;
    int evaluations_ = 0;
    int signals_generated_ = 0;
};

} // namespace trading
