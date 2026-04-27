#include "StateStore.h"

namespace trading {

void StateStore::update_btc_price(const PriceTick& tick) {
    std::unique_lock lock(btc_mutex_);
    latest_btc_tick_ = tick;
    btc_initialized_ = true;
}

std::optional<PriceTick> StateStore::get_latest_btc_price() const {
    std::shared_lock lock(btc_mutex_);
    if (!btc_initialized_) {
        return std::nullopt;
    }
    return latest_btc_tick_;
}

void StateStore::update_token_price(std::string_view token_id, const TokenPrice& price) {
    std::unique_lock lock(token_mutex_);
    token_prices_[std::string(token_id)] = price;
}

std::optional<TokenPrice> StateStore::get_token_price(std::string_view token_id) const {
    std::shared_lock lock(token_mutex_);
    auto it = token_prices_.find(std::string(token_id));
    if (it != token_prices_.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace trading
