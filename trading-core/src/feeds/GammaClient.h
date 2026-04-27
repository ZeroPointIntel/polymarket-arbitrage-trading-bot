#pragma once
#include <string>
#include <vector>
#include <optional>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "../signals/Signal.h"

namespace trading {

class GammaClient {
public:
    GammaClient(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx);

    // Synchronously fetches the first active BTC 5-minute up/down market
    // Returns nullopt if none found or on error.
    std::optional<MarketInfo> fetch_active_btc_market();

private:
    boost::asio::io_context& ioc_;
    boost::asio::ssl::context& ctx_;
};

} // namespace trading
