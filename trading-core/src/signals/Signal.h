#pragma once
#include <string>

namespace trading {

struct MarketInfo {
    std::string condition_id;
    std::string question;
    std::string asset;
    std::string yes_token_id;
    std::string no_token_id;
    double strike;
    double end_date_ts; // UNIX timestamp in seconds
};

struct DumpHedgeSignal {
    MarketInfo market;
    std::string asset;
    std::string yes_token_id;
    std::string no_token_id;
    double yes_price;
    double no_price;
    double combined_price;
    double discount;
    double discount_pct;
    double seconds_remaining;
    double timestamp;
};

struct LatencyArbSignal {
    MarketInfo market;
    std::string asset;
    std::string token_id;
    std::string side; // "BUY" or "SELL"
    double polymarket_price;
    double binance_price;
    double fair_value;
    double edge;
    double seconds_remaining;
    double timestamp;
};

} // namespace trading
