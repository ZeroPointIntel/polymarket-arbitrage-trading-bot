#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

namespace trading {

struct PriceTick {
    double price;
    double timestamp_ms;
    double volume;
    double received_at;
};

struct TokenPrice {
    double price;
    std::string side; // "BUY" or "SELL"
    double ts;
};

class StateStore {
public:
    void update_btc_price(const PriceTick& tick);
    std::optional<PriceTick> get_latest_btc_price() const;

    void update_token_price(std::string_view token_id, const TokenPrice& price);
    std::optional<TokenPrice> get_token_price(std::string_view token_id) const;

private:
    mutable std::shared_mutex btc_mutex_;
    PriceTick latest_btc_tick_{};
    bool btc_initialized_ = false;

    mutable std::shared_mutex token_mutex_;
    std::unordered_map<std::string, TokenPrice> token_prices_;
};

} // namespace trading
