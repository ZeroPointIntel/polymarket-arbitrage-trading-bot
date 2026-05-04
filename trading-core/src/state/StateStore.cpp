#include "StateStore.h"
#include <mutex>
#include <boost/json.hpp>
#include <algorithm>
#include <chrono>

namespace trading {

void StateStore::push_telemetry(const std::string& line) {
    std::unique_lock lock(log_mutex_);
    telemetry_log_.push_back(line);
    if (telemetry_log_.size() > MAX_LOG_LINES) telemetry_log_.pop_front();
}

void StateStore::push_signal(const std::string& line) {
    std::unique_lock lock(log_mutex_);
    signal_log_.push_back(line);
    if (signal_log_.size() > MAX_LOG_LINES) signal_log_.pop_front();
}

void StateStore::update_btc_price(const PriceTick& tick) {
    std::unique_lock lock(btc_mutex_);
    latest_btc_tick_ = tick;
    btc_tick_count_++;
    btc_history_.push_back(tick);
    if (btc_history_.size() > 5000) btc_history_.pop_front();
}

std::optional<PriceTick> StateStore::get_latest_btc_price() const {
    std::shared_lock lock(btc_mutex_);
    return latest_btc_tick_;
}

void StateStore::update_eth_price(const PriceTick& tick) {
    std::unique_lock lock(eth_mutex_);
    latest_eth_tick_ = tick;
    eth_tick_count_++;
    eth_history_.push_back(tick);
    if (eth_history_.size() > 5000) eth_history_.pop_front();
}

std::optional<PriceTick> StateStore::get_latest_eth_price() const {
    std::shared_lock lock(eth_mutex_);
    return latest_eth_tick_;
}

void StateStore::update_sol_price(const PriceTick& tick) {
    std::unique_lock lock(sol_mutex_);
    latest_sol_tick_ = tick;
    sol_tick_count_++;
    sol_history_.push_back(tick);
    if (sol_history_.size() > 5000) sol_history_.pop_front();
}

std::optional<PriceTick> StateStore::get_latest_sol_price() const {
    std::shared_lock lock(sol_mutex_);
    return latest_sol_tick_;
}

std::optional<double> StateStore::get_price_at(const std::string& asset, double seconds_ago) const {
    const std::deque<PriceTick>* history = nullptr;
     std::shared_mutex* mutex = nullptr;

    if (asset == "eth") { history = &eth_history_; mutex = &eth_mutex_; }
    else if (asset == "sol") { history = &sol_history_; mutex = &sol_mutex_; }
    else { history = &btc_history_; mutex = &btc_mutex_; }

    std::shared_lock lock(*mutex);
    if (history->empty()) return std::nullopt;
    
    double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    double target = now - seconds_ago;
    
    for (auto it = history->rbegin(); it != history->rend(); ++it) {
        if (it->received_at <= target) return it->price;
    }
    return history->front().price;
}

PriceTick StateStore::get_latest_price(const std::string& asset) const {
    if (asset == "eth") { std::shared_lock lock(eth_mutex_); return latest_eth_tick_; }
    else if (asset == "sol") { std::shared_lock lock(sol_mutex_); return latest_sol_tick_; }
    else { std::shared_lock lock(btc_mutex_); return latest_btc_tick_; }
}

void StateStore::update_token_price(std::string_view token_id, const TokenPrice& price) {
    std::unique_lock lock(token_mutex_);
    token_prices_[std::string(token_id)] = price;
}

std::optional<TokenPrice> StateStore::get_token_price(std::string_view token_id) const {
    std::shared_lock lock(token_mutex_);
    auto it = token_prices_.find(std::string(token_id));
    if (it != token_prices_.end()) return it->second;
    return std::nullopt;
}

void StateStore::update_markets(const std::vector<MarketInfo>& markets) {
    std::unique_lock lock(market_mutex_);
    markets_ = markets;
}

std::string StateStore::get_dashboard_json() const {
    boost::json::object root;
    
    auto add_asset_data = [&](const std::string& sym, double price, uint64_t count) {
        boost::json::object obj;
        obj["price"] = price;
        obj["count"] = count;
        auto p27 = get_price_at(sym, 2.7);
        auto p60 = get_price_at(sym, 60.0);
        obj["delta27"] = p27 ? (price - *p27) : 0.0;
        obj["delta60"] = p60 ? (price - *p60) : 0.0;
        root[sym + "Data"] = std::move(obj);
        root[sym + "Price"] = price; // Compat
    };

    { 
        std::shared_lock lock(btc_mutex_); 
        double p = latest_btc_tick_.price; 
        uint64_t c = btc_tick_count_; 
        lock.unlock(); // Release lock before calling helpers that take the lock
        add_asset_data("btc", p, c); 
    }
    { 
        std::shared_lock lock(eth_mutex_); 
        double p = latest_eth_tick_.price; 
        uint64_t c = eth_tick_count_; 
        lock.unlock();
        add_asset_data("eth", p, c); 
    }
    { 
        std::shared_lock lock(sol_mutex_); 
        double p = latest_sol_tick_.price; 
        uint64_t c = sol_tick_count_; 
        lock.unlock();
        add_asset_data("sol", p, c); 
    }
    
    if (risk_manager_) {
        double balance = risk_manager_->get_current_balance();
        double daily_start = risk_manager_->get_daily_starting_balance();
        double peak = risk_manager_->get_peak_balance();
        double daily_pnl = balance - daily_start;
        double total_pnl = balance - 1000.0; // Paper start $1000
        double drawdown = peak > 0 ? (peak - balance) / peak * 100.0 : 0.0;

        root["balance"] = balance;
        root["dailyStartingBalance"] = daily_start;
        root["peakBalance"] = peak;
        root["dailyPnl"] = daily_pnl;
        root["totalPnl"] = total_pnl;
        root["maxDrawdownPct"] = drawdown;
        root["openCount"] = risk_manager_->get_open_position_count();
        root["totalTrades"] = risk_manager_->get_total_trades();
        root["totalDhTrades"] = risk_manager_->get_total_dh_trades();
        root["winRate"] = risk_manager_->get_win_rate() * 100.0;
        root["laPnl"] = risk_manager_->get_la_pnl();
        root["dhPnl"] = risk_manager_->get_dh_pnl();
        root["status"] = static_cast<int>(risk_manager_->get_status());

        boost::json::array pos_arr;
        for (const auto& [id, p] : risk_manager_->get_open_positions()) {
            boost::json::object po;
            po["asset"] = p.asset.c_str();
            po["side"] = p.side.c_str();
            po["entryPrice"] = p.entry_price;
            po["size"] = p.size_shares;
            po["cost"] = p.cost_usdc;
            po["strategy"] = p.strategy.c_str();
            po["question"] = p.market_question.c_str();
            // Compute unrealised PnL from live token price
            auto live = get_token_price(p.token_id);
            double unrealised = live ? (live->price - p.entry_price) * p.size_shares : 0.0;
            po["pnl"] = unrealised;
            pos_arr.push_back(po);
        }
        for (const auto& [id, p] : risk_manager_->get_open_dh_positions()) {
            boost::json::object po;
            po["asset"] = p.asset.c_str();
            po["side"] = "DUAL";
            po["entryPrice"] = p.combined_entry_price;
            po["size"] = p.size_shares;
            po["cost"] = p.combined_cost_usdc;
            po["strategy"] = "DH";
            po["question"] = p.market_question.c_str();
            po["pnl"] = p.locked_profit_usdc;
            pos_arr.push_back(po);
        }
        root["openPositions"] = std::move(pos_arr);
    } else {
        root["balance"] = 1000.0;
        root["dailyStartingBalance"] = 1000.0;
        root["peakBalance"] = 1000.0;
        root["dailyPnl"] = 0.0;
        root["totalPnl"] = 0.0;
        root["maxDrawdownPct"] = 0.0;
        root["openCount"] = 0;
        root["totalTrades"] = 0;
        root["totalDhTrades"] = 0;
        root["winRate"] = 0.0;
        root["laPnl"] = 0.0;
        root["dhPnl"] = 0.0;
        root["status"] = 0;
        root["openPositions"] = boost::json::array{};
    }

    boost::json::array opps;
    { 
        std::shared_lock lock(market_mutex_); 
        std::shared_lock t_lock(token_mutex_);
        for (const auto& m : markets_) {
            boost::json::object mo;
            mo["asset"] = m.asset.c_str();
            mo["question"] = m.question.c_str();
            auto it_y = token_prices_.find(m.yes_token_id);
            auto it_n = token_prices_.find(m.no_token_id);
            double yes = 0.0;
            double no = 0.0;
            double combined = 1.0;
            double discountPct = 0.0;

            if (it_y != token_prices_.end() && it_n != token_prices_.end()) {
                yes = it_y->second.price;
                no = it_n->second.price;
                combined = yes + no;
                if (combined < 1.0 && combined > 0.0) {
                    discountPct = (1.0 - combined) * 100.0;
                }
            }

            mo["yesPrice"] = yes; 
            mo["noPrice"] = no;
            mo["combined"] = combined;
            mo["discountPct"] = discountPct;
            opps.push_back(mo);
        }
    }
    root["dhOpportunities"] = std::move(opps);

    // Telemetry & signal logs
    {
        std::shared_lock lock(log_mutex_);
        boost::json::array tlog;
        for (const auto& l : telemetry_log_) tlog.push_back(l.c_str());
        root["telemetryLog"] = std::move(tlog);

        boost::json::array slog;
        for (const auto& l : signal_log_) slog.push_back(l.c_str());
        root["signalLog"] = std::move(slog);
    }

    // Per-asset tick rates (ticks/sec over last second approximation)
    {
        std::shared_lock lb(btc_mutex_);
        root["btcTickRate"] = static_cast<double>(btc_tick_count_);
    }
    {
        std::shared_lock le(eth_mutex_);
        root["ethTickRate"] = static_cast<double>(eth_tick_count_);
    }
    {
        std::shared_lock ls(sol_mutex_);
        root["solTickRate"] = static_cast<double>(sol_tick_count_);
    }

    return boost::json::serialize(root);
}


void StateStore::update_token_bid(std::string_view token_id, const TokenPrice& price) {
    std::unique_lock lock(token_mutex_);
    token_bids_[std::string(token_id)] = price;
}

std::optional<TokenPrice> StateStore::get_token_bid(std::string_view token_id) const {
    std::shared_lock lock(token_mutex_);
    auto it = token_bids_.find(std::string(token_id));
    if (it != token_bids_.end()) return it->second;
    return std::nullopt;
}

} // namespace trading
