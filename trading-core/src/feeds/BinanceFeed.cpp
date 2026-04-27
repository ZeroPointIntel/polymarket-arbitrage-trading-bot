#include "BinanceFeed.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace trading {

BinanceFeed::BinanceFeed(net::io_context& ioc, ssl::context& ctx, StateStore& store, std::string symbol)
    : resolver_(net::make_strand(ioc)),
      ws_(net::make_strand(ioc), ctx),
      timer_(net::make_strand(ioc)),
      store_(store),
      symbol_(std::move(symbol)) {
    
    std::string lower_sym = symbol_;
    std::transform(lower_sym.begin(), lower_sym.end(), lower_sym.begin(), [](unsigned char c){ return std::tolower(c); });
    path_ = "/ws/" + lower_sym + "@trade";
}

BinanceFeed::~BinanceFeed() = default;

void BinanceFeed::start() {
    running_ = true;
    resolve();
}

void BinanceFeed::stop() {
    running_ = false;
    timer_.cancel();
    if (ws_.is_open()) {
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
    }
}

void BinanceFeed::resolve() {
    if (!running_) return;
    spdlog::info("BinanceFeed: Resolving {}...", host_);
    resolver_.async_resolve(
        host_,
        port_,
        beast::bind_front_handler(&BinanceFeed::on_resolve, shared_from_this()));
}

void BinanceFeed::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        spdlog::error("BinanceFeed: Resolve error: {}", ec.message());
        return reconnect();
    }
    
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(10));
    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&BinanceFeed::on_connect, shared_from_this()));
}

void BinanceFeed::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    if (ec) {
        spdlog::error("BinanceFeed: Connect error: {}", ec.message());
        return reconnect();
    }

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(10));

    if(! SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
        beast::error_code err{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        spdlog::error("BinanceFeed: SNI error: {}", err.message());
        return reconnect();
    }

    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&BinanceFeed::on_ssl_handshake, shared_from_this()));
}

void BinanceFeed::on_ssl_handshake(beast::error_code ec) {
    if (ec) {
        spdlog::error("BinanceFeed: SSL handshake error: {}", ec.message());
        return reconnect();
    }

    beast::get_lowest_layer(ws_).expires_never();
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        }));

    ws_.async_handshake(host_, path_,
        beast::bind_front_handler(&BinanceFeed::on_handshake, shared_from_this()));
}

void BinanceFeed::on_handshake(beast::error_code ec) {
    if (ec) {
        spdlog::error("BinanceFeed: WebSocket handshake error: {}", ec.message());
        return reconnect();
    }

    spdlog::info("BinanceFeed: Connected to {} {}", host_, path_);
    do_read();
}

void BinanceFeed::do_read() {
    if (!running_) return;
    ws_.async_read(buffer_,
        beast::bind_front_handler(&BinanceFeed::on_read, shared_from_this()));
}

void BinanceFeed::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        spdlog::warn("BinanceFeed: Read error: {}", ec.message());
        return reconnect();
    }

    auto data = beast::buffers_to_string(buffer_.data());
    process_message(data);
    
    buffer_.consume(buffer_.size());
    do_read();
}

void BinanceFeed::process_message(std::string_view msg) {
    try {
        // Zero-allocation path requires padded string view if simdjson dictates it,
        // but for now we use parse directly with padding enabled by default in simdjson 3+.
        simdjson::padded_string json(msg);
        simdjson::ondemand::document doc = parser_.iterate(json);
        
        std::string_view event_type = doc["e"].get_string();
        if (event_type == "trade") {
            PriceTick tick;
            
            // "p" comes as a string in Binance, we need to convert
            std::string_view price_str = doc["p"].get_string();
            tick.price = std::stod(std::string(price_str));
            
            tick.timestamp_ms = doc["T"].get_uint64();
            
            std::string_view qty_str = doc["q"].get_string();
            tick.volume = std::stod(std::string(qty_str));
            
            auto now = std::chrono::system_clock::now();
            tick.received_at = std::chrono::duration<double>(now.time_since_epoch()).count();

            store_.update_btc_price(tick);
            
            // Log sample
            static int count = 0;
            if (++count % 500 == 0) {
                spdlog::debug("BinanceFeed: tick #{} - ${:.2f}", count, tick.price);
            }
        }
    } catch (const simdjson::simdjson_error& e) {
        spdlog::warn("BinanceFeed: JSON parse error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::warn("BinanceFeed: Error processing message: {}", e.what());
    }
}

void BinanceFeed::reconnect() {
    if (!running_) return;
    spdlog::info("BinanceFeed: Reconnecting in 2 seconds...");
    
    beast::error_code ec;
    ws_.next_layer().next_layer().close(ec);

    timer_.expires_after(std::chrono::seconds(2));
    timer_.async_wait([this](beast::error_code ec) {
        if (!ec && running_) {
            resolve();
        }
    });
}

} // namespace trading
