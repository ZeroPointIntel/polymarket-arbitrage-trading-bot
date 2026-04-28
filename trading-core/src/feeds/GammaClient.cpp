#include "GammaClient.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <spdlog/spdlog.h>
#include <regex>
#include <algorithm>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace trading {

GammaClient::GammaClient(net::io_context& ioc, ssl::context& ctx)
    : ioc_(ioc), ctx_(ctx) {}

std::string GammaClient::http_get(const std::string& host, const std::string& target) {
    tcp::resolver resolver(ioc_);
    beast::ssl_stream<beast::tcp_stream> stream(ioc_, ctx_);

    if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        throw boost::system::system_error{ec};
    }

    auto const results = resolver.resolve(host, "443");
    beast::get_lowest_layer(stream).connect(results);
    stream.handshake(ssl::stream_base::client);

    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    beast::error_code ec;
    stream.shutdown(ec);
    // Ignore EOF / not_connected on shutdown

    return res.body();
}

std::optional<MarketInfo> GammaClient::fetch_active_btc_market() {
    try {
        std::string host = "gamma-api.polymarket.com";
        std::string target = "/events?active=true&closed=false&limit=20";

        auto body = http_get(host, target);
        auto jv = boost::json::parse(body);
        if (!jv.is_array()) {
            spdlog::error("Gamma API response is not an array");
            return std::nullopt;
        }

        for (const auto& item : jv.as_array()) {
            const auto& event = item.as_object();
            if (!event.contains("slug") || !event.at("slug").is_string()) continue;
            
            std::string slug(event.at("slug").as_string());
            if (slug.find("btc-updown-5m") != std::string::npos) {
                if (!event.contains("markets") || !event.at("markets").is_array()) continue;
                auto markets = event.at("markets").as_array();
                if (markets.empty()) continue;

                const auto& market = markets[0].as_object();
                if (!market.contains("conditionId") || !market.contains("tokens")) continue;

                MarketInfo info;
                info.condition_id = std::string(market.at("conditionId").as_string());
                info.question = std::string(market.at("question").as_string());
                info.asset = "btc";

                auto tokens = market.at("tokens").as_array();
                if (tokens.size() >= 2) {
                    info.yes_token_id = std::string(tokens[0].as_object().at("token_id").as_string());
                    info.no_token_id = std::string(tokens[1].as_object().at("token_id").as_string());
                } else {
                    continue;
                }

                // Extract strike price via regex from question e.g., "Bitcoin Up or Down - 5 Minutes? (Above $68,500)"
                std::regex strike_regex(R"(\$([0-9,]+(\.[0-9]+)?))");
                std::smatch match;
                if (std::regex_search(info.question, match, strike_regex)) {
                    std::string strike_str = match[1].str();
                    // Remove commas
                    strike_str.erase(std::remove(strike_str.begin(), strike_str.end(), ','), strike_str.end());
                    info.strike = std::stod(strike_str);
                } else {
                    // Fallback or skip
                    spdlog::warn("Could not extract strike from question: {}", info.question);
                    info.strike = 0.0;
                }

                // Approximate end date using 5-minute align
                double now_ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
                int ws = 300;
                int window_open = (static_cast<int>(now_ts) / ws) * ws;
                info.end_date_ts = window_open + ws;

                return info;
            }
        }
        
        spdlog::warn("btc-updown-5m slug not found, use fetch_all_binary_markets() instead.");
        return std::nullopt;

    } catch(std::exception const& e) {
        spdlog::error("GammaClient error: {}", e.what());
        return std::nullopt;
    }
}

std::vector<MarketInfo> GammaClient::fetch_all_binary_markets(int limit) {
    std::vector<MarketInfo> results;
    
    try {
        std::string host = "gamma-api.polymarket.com";
        int batch_size = 100; // Gamma API max per request
        int offset = 0;
        int total_fetched = 0;
        
        while (total_fetched < limit) {
            int fetch_count = std::min(batch_size, limit - total_fetched);
            std::string target = "/events?active=true&closed=false&limit=" 
                + std::to_string(fetch_count) + "&offset=" + std::to_string(offset);
            
            auto body = http_get(host, target);
            auto jv = boost::json::parse(body);
            
            if (!jv.is_array()) {
                spdlog::error("GammaClient: Unexpected response at offset {}", offset);
                break;
            }
            
            auto events = jv.as_array();
            if (events.empty()) break; // No more events
            
            for (const auto& item : events) {
                if (!item.is_object()) continue;
                const auto& event = item.as_object();
                
                std::string slug = event.contains("slug") && event.at("slug").is_string() 
                    ? std::string(event.at("slug").as_string()) : "";
                
                if (!event.contains("markets") || !event.at("markets").is_array()) continue;
                
                for (const auto& market_val : event.at("markets").as_array()) {
                    if (!market_val.is_object()) continue;
                    const auto& market = market_val.as_object();
                    
                    // Must have conditionId and clobTokenIds
                    if (!market.contains("conditionId") || !market.contains("clobTokenIds")) continue;
                    
                    // Parse clobTokenIds — stringified JSON array e.g. "[\"abc\", \"def\"]"
                    std::string clob_str;
                    if (market.at("clobTokenIds").is_string()) {
                        clob_str = std::string(market.at("clobTokenIds").as_string());
                    } else {
                        continue;
                    }
                    
                    boost::json::value tokens_jv;
                    try {
                        tokens_jv = boost::json::parse(clob_str);
                    } catch (...) {
                        continue;
                    }
                    if (!tokens_jv.is_array()) continue;
                    auto tokens = tokens_jv.as_array();
                    if (tokens.size() < 2) continue;
                    
                    // Parse outcome prices to filter dead markets
                    double yes_price = 0.0, no_price = 0.0;
                    if (market.contains("outcomePrices") && market.at("outcomePrices").is_string()) {
                        try {
                            auto prices_jv = boost::json::parse(
                                std::string(market.at("outcomePrices").as_string()));
                            if (prices_jv.is_array() && prices_jv.as_array().size() >= 2) {
                                yes_price = std::stod(std::string(prices_jv.as_array()[0].as_string()));
                                no_price = std::stod(std::string(prices_jv.as_array()[1].as_string()));
                            }
                        } catch (...) {}
                    }
                    
                    // Skip dead markets where either side is essentially zero
                    if (yes_price < 0.01 || no_price < 0.01) continue;
                    
                    MarketInfo info;
                    info.condition_id = std::string(market.at("conditionId").as_string());
                    info.question = market.contains("question") && market.at("question").is_string()
                        ? std::string(market.at("question").as_string()) : slug;
                    info.yes_token_id = std::string(tokens[0].as_string());
                    info.no_token_id = std::string(tokens[1].as_string());
                    info.asset = slug;
                    info.strike = 0.0; // Not relevant for generic DH
                    
                    // Parse endDate ISO string
                    if (market.contains("endDate") && market.at("endDate").is_string()) {
                        info.end_date_iso = std::string(market.at("endDate").as_string());
                        // Approximate end_date_ts: for now use a far-future default
                        // (exact parsing of ISO dates is complex in C++ without a lib)
                        info.end_date_ts = std::chrono::duration<double>(
                            std::chrono::system_clock::now().time_since_epoch()).count() + 86400 * 365;
                    } else {
                        info.end_date_ts = std::chrono::duration<double>(
                            std::chrono::system_clock::now().time_since_epoch()).count() + 86400 * 365;
                    }
                    
                    // Parse volume for sorting
                    if (market.contains("volumeNum") && market.at("volumeNum").is_double()) {
                        info.volume = market.at("volumeNum").as_double();
                    } else if (market.contains("volumeNum") && market.at("volumeNum").is_int64()) {
                        info.volume = static_cast<double>(market.at("volumeNum").as_int64());
                    }
                    
                    results.push_back(std::move(info));
                }
            }
            
            total_fetched += static_cast<int>(events.size());
            offset += static_cast<int>(events.size());
            
            // If we got fewer than requested, we've exhausted the API
            if (static_cast<int>(events.size()) < fetch_count) break;
        }
        
        // Sort by volume descending — highest liquidity first
        std::sort(results.begin(), results.end(), 
            [](const MarketInfo& a, const MarketInfo& b) { return a.volume > b.volume; });
        
        spdlog::info("GammaClient: Discovered {} active binary markets across {} events", 
                     results.size(), total_fetched);
        
    } catch (const std::exception& e) {
        spdlog::error("GammaClient::fetch_all_binary_markets error: {}", e.what());
    }
    
    return results;
}

} // namespace trading
