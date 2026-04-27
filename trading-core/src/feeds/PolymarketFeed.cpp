#include "PolymarketFeed.h"
#include <spdlog/spdlog.h>
#include <boost/json.hpp>
#include <chrono>

namespace trading {

PolymarketFeed::PolymarketFeed(net::io_context& ioc, ssl::context& ctx, StateStore& store)
    : resolver_(net::make_strand(ioc)),
      ioc_(ioc),
      ctx_(ctx),
      ws_(std::in_place, net::make_strand(ioc), ctx),
      timer_(net::make_strand(ioc)),
      ping_timer_(net::make_strand(ioc)),
      store_(store) {}

PolymarketFeed::~PolymarketFeed() = default;

void PolymarketFeed::start() {
    running_ = true;
    resolve();
}

void PolymarketFeed::stop() {
    running_ = false;
    connected_ = false;
    timer_.cancel();
    ping_timer_.cancel();
    if (ws_ && ws_->is_open()) {
        beast::error_code ec;
        ws_->close(websocket::close_code::normal, ec);
    }
}

void PolymarketFeed::subscribe(const std::vector<std::string>& token_ids) {
    for (const auto& token : token_ids) {
        subscribed_tokens_.push_back(token);
    }
    
    if (connected_) {
        // Build JSON string using standard string appending for simplicity here, 
        // normally use a JSON builder.
        std::string json = "{\"type\":\"market\",\"assets_ids\":[";
        for (size_t i = 0; i < subscribed_tokens_.size(); ++i) {
            json += "\"" + subscribed_tokens_[i] + "\"";
            if (i < subscribed_tokens_.size() - 1) json += ",";
        }
        json += "]}";
        
        if (ws_) {
            ws_->async_write(net::buffer(json), [](beast::error_code ec, std::size_t) {
                if (ec) {
                    spdlog::warn("PolymarketFeed: Failed to subscribe: {}", ec.message());
                } else {
                    spdlog::info("PolymarketFeed: Subscription sent.");
                }
            });
        }
    }
}

void PolymarketFeed::resolve() {
    if (!running_) return;
    spdlog::info("PolymarketFeed: Resolving {}...", host_);
    resolver_.async_resolve(
        host_,
        port_,
        beast::bind_front_handler(&PolymarketFeed::on_resolve, shared_from_this()));
}

void PolymarketFeed::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        spdlog::error("PolymarketFeed: Resolve error: {}", ec.message());
        return reconnect();
    }
    
    beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(10));
    beast::get_lowest_layer(*ws_).async_connect(
        results,
        beast::bind_front_handler(&PolymarketFeed::on_connect, shared_from_this()));
}

void PolymarketFeed::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    if (ec) {
        spdlog::error("PolymarketFeed: Connect error: {}", ec.message());
        return reconnect();
    }

    beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(10));

    if(! SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str())) {
        beast::error_code err{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        spdlog::error("PolymarketFeed: SNI error: {}", err.message());
        return reconnect();
    }

    ws_->next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&PolymarketFeed::on_ssl_handshake, shared_from_this()));
}

void PolymarketFeed::on_ssl_handshake(beast::error_code ec) {
    if (ec) {
        spdlog::error("PolymarketFeed: SSL handshake error: {}", ec.message());
        return reconnect();
    }

    beast::get_lowest_layer(*ws_).expires_never();
    
    // Polymarket requires specific headers
    ws_->set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::user_agent, "Mozilla/5.0 (C++ Trading Core)");
            req.set(http::field::origin, "https://polymarket.com");
        }));

    ws_->async_handshake(host_, path_,
        beast::bind_front_handler(&PolymarketFeed::on_handshake, shared_from_this()));
}

void PolymarketFeed::on_handshake(beast::error_code ec) {
    if (ec) {
        spdlog::error("PolymarketFeed: WebSocket handshake error: {}", ec.message());
        return reconnect();
    }

    spdlog::info("PolymarketFeed: Connected to {} {}", host_, path_);
    connected_ = true;
    
    // Resubscribe if needed
    if (!subscribed_tokens_.empty()) {
        subscribe({}); // empty means just send current list
    }
    
    // Start ping loop
    send_ping();
    
    do_read();
}

void PolymarketFeed::send_ping() {
    if (!running_ || !connected_) return;
    
    ping_timer_.expires_after(std::chrono::seconds(5));
    ping_timer_.async_wait([this](beast::error_code ec) {
        if (!ec && running_ && connected_) {
            if (ws_) {
                ws_->async_write(net::buffer(std::string_view("{\"type\":\"ping\"}")), 
                    [this](beast::error_code write_ec, std::size_t) {
                        if (write_ec) {
                            spdlog::warn("PolymarketFeed: Ping failed: {}", write_ec.message());
                        } else {
                            send_ping();
                        }
                    });
            }
        }
    });
}

void PolymarketFeed::do_read() {
    if (!running_) return;
    if (ws_) {
        ws_->async_read(buffer_,
            beast::bind_front_handler(&PolymarketFeed::on_read, shared_from_this()));
    }
}

void PolymarketFeed::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        spdlog::warn("PolymarketFeed: Read error: {}", ec.message());
        return reconnect();
    }

    auto data = beast::buffers_to_string(buffer_.data());
    process_message(data);
    
    buffer_.consume(buffer_.size());
    do_read();
}

void PolymarketFeed::process_message(std::string_view msg) {
    try {
        boost::json::value jv = boost::json::parse(msg);
        
        // Polymarket can send an array or a single object. We handle array of events.
        if (jv.is_array()) {
            for (const auto& item_val : jv.as_array()) {
                if (!item_val.is_object()) continue;
                const auto& item = item_val.as_object();
                
                if (item.contains("event_type") && item.at("event_type").is_string()) {
                    auto event_type = item.at("event_type").as_string();
                    if (event_type == "price_change") {
                        if (item.contains("asset_id") && item.contains("price") && item.contains("side")) {
                            std::string asset_id(item.at("asset_id").as_string());
                            std::string price_str(item.at("price").as_string());
                            std::string side(item.at("side").as_string());
                            
                            TokenPrice tp;
                            tp.price = std::stod(price_str);
                            
                            // Inverse the side relative to the Maker
                            tp.side = (side == "BUY") ? "SELL" : "BUY";
                            
                            auto now = std::chrono::system_clock::now();
                            tp.ts = std::chrono::duration<double>(now.time_since_epoch()).count();
                            
                            store_.update_token_price(asset_id, tp);
                        }
                    }
                }
            }
        }
    } catch (const boost::json::system_error& e) {
        // Not a big deal, could be a ping response or other non-json message
    } catch (const std::exception& e) {
        spdlog::warn("PolymarketFeed: Error processing message: {}", e.what());
    }
}

void PolymarketFeed::reconnect() {
    if (!running_) return;
    connected_ = false;
    spdlog::info("PolymarketFeed: Reconnecting in 2 seconds...");
    
    if (ws_) {
        ws_->next_layer().next_layer().close();
    }
    ping_timer_.cancel();

    timer_.expires_after(std::chrono::seconds(2));
    timer_.async_wait([this](beast::error_code ec) {
        if (!ec && running_) {
            ws_.emplace(net::make_strand(ioc_), ctx_);
            resolve();
        }
    });
}

} // namespace trading
