#pragma once
#include "Signal.h"
#include "../state/StateStore.h"
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <unordered_map>

namespace trading {

struct AssetConfig {
    double base_scale;
    double min_scale;
    double min_price_move;
};

class LatencyArbDetector {
public:
    // price_resolver: optional REST fallback — mirrors Python get_market_price().
    // Signature: (token_id, side) -> optional<double>.
    // When provided, called only when WS cache misses (nullopt or stale).
    // MUST be thread-safe — runs on feed_ioc thread.
    using PriceResolver = std::function<std::optional<double>(const std::string&, const std::string&)>;

    LatencyArbDetector(StateStore& state_store, 
                       std::vector<MarketInfo> active_markets,
                       double min_edge_threshold = 0.04,
                       double min_seconds_remaining = 60.0,
                       double cooldown_seconds = 15.0,
                       double lag_window_seconds = 2.7,
                       std::string asset = "btc",
                       PriceResolver price_resolver = nullptr);

    std::optional<LatencyArbSignal> evaluate(double current_time_ms);
    void set_active_markets(std::vector<MarketInfo> markets) { active_markets_ = std::move(markets); }
    void reset_cooldown(const std::string& asset, double current_time_ms);

    // Runtime tuning — mirrors DH detector pattern
    void set_min_edge(double val)          { min_edge_threshold_ = val; }
    void set_entry_price_range(double lo, double hi) { min_entry_price_ = lo; max_entry_price_ = hi; }
    void set_min_fair_value_strength(double val)     { min_fair_value_strength_ = val; }
    void set_window_seconds(double val)    { window_seconds_ = val; }

private:
    double compute_fair_value_5m(double price_now, double price_to_beat, 
                                 double seconds_remaining, const std::string& direction,
                                 const AssetConfig& cfg);

    StateStore& state_store_;
    std::vector<MarketInfo> active_markets_;
    double min_edge_threshold_;
    double min_seconds_remaining_;
    double cooldown_seconds_;
    double lag_window_seconds_;
    std::string asset_;

    std::unordered_map<std::string, AssetConfig> asset_configs_;
    std::unordered_map<std::string, double> last_signal_time_;
    PriceResolver price_resolver_;

    // Strict filters — configurable via setters
    double min_entry_price_ = 0.25;
    double max_entry_price_ = 0.75;
    double min_fair_value_strength_ = 0.08;
    double window_seconds_ = 300.0;
    
    double last_price_now_ = 0.0;
    double last_price_time_ = 0.0;

    int evaluations_ = 0;
    int signals_generated_ = 0;
};

} // namespace trading
