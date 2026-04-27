#include "GammaClient.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <spdlog/spdlog.h>
#include <regex>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace trading {

GammaClient::GammaClient(net::io_context& ioc, ssl::context& ctx)
    : ioc_(ioc), ctx_(ctx) {}

std::optional<MarketInfo> GammaClient::fetch_active_btc_market() {
    try {
        std::string host = "gamma-api.polymarket.com";
        std::string port = "443";
        std::string target = "/events?active=true&closed=false&limit=20";

        tcp::resolver resolver(ioc_);
        beast::ssl_stream<beast::tcp_stream> stream(ioc_, ctx_);

        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
            throw boost::system::system_error{ec};
        }

        auto const results = resolver.resolve(host, port);
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
        if(ec && ec != beast::errc::not_connected) {
            // Ignore EOF
        }

        auto jv = boost::json::parse(res.body());
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
        
        // If we reach here and didn't find btc-updown-5m, let's just grab the first valid market to test the feeds.
        for (const auto& item : jv.as_array()) {
            const auto& event = item.as_object();
            if (!event.contains("markets") || !event.at("markets").is_array()) continue;
            auto markets = event.at("markets").as_array();
            if (markets.empty()) continue;

            const auto& market = markets[0].as_object();
            if (!market.contains("conditionId") || !market.contains("clobTokenIds")) continue;

            auto tokens_str = std::string(market.at("clobTokenIds").as_string());
            // It's a stringified JSON array like "[\"123\", \"456\"]"
            auto tokens_jv = boost::json::parse(tokens_str);
            if (!tokens_jv.is_array()) continue;
            auto tokens = tokens_jv.as_array();
            
            if (tokens.size() >= 2) {
                MarketInfo info;
                info.condition_id = std::string(market.at("conditionId").as_string());
                info.question = std::string(market.at("question").as_string());
                info.asset = "fallback";
                info.yes_token_id = std::string(tokens[0].as_string());
                info.no_token_id = std::string(tokens[1].as_string());
                info.strike = 0.0;
                info.end_date_ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count() + 300;
                
                spdlog::warn("Using fallback market for testing: {}", info.question);
                return info;
            }
        }
        
        spdlog::warn("No active market found at all in Gamma response.");
        return std::nullopt;

    } catch(std::exception const& e) {
        spdlog::error("GammaClient error: {}", e.what());
        return std::nullopt;
    }
}

} // namespace trading
