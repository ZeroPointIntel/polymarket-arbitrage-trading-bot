#include "LiveStateServer.h"
#include <spdlog/spdlog.h>
#include <boost/beast/http.hpp>

namespace trading {
namespace control {

namespace http = boost::beast::http;

LiveStateSession::LiveStateSession(tcp::socket&& socket, LiveStateServer& server)
    : ws_(std::move(socket)), server_(server) {}

LiveStateSession::~LiveStateSession() {
    server_.remove_session(this);
}

void LiveStateSession::run() {
    // Set suggested timeout settings for the websocket
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " polymarket-bot-live");
        }));

    // Accept the websocket handshake
    ws_.async_accept(
        beast::bind_front_handler(&LiveStateSession::on_accept, shared_from_this()));
}

void LiveStateSession::on_accept(beast::error_code ec) {
    if (ec) {
        spdlog::warn("LiveStateSession accept error: {}", ec.message());
        return;
    }
    
    server_.add_session(shared_from_this());
    do_read();
}

void LiveStateSession::do_read() {
    // Read a message just to detect disconnects
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&LiveStateSession::on_read, shared_from_this()));
}

void LiveStateSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed || ec) {
        return; // Will destruct and remove from server
    }

    buffer_.consume(buffer_.size());
    do_read();
}

void LiveStateSession::send_message(std::shared_ptr<std::string> msg) {
    queue_.push_back(std::move(msg));
    
    if (queue_.size() > 10) {
        // Drop old messages to prevent memory explosion if client is slow
        queue_.erase(queue_.begin(), queue_.end() - 1);
    }

    if (!writing_) {
        writing_ = true;
        ws_.async_write(
            net::buffer(*queue_.front()),
            beast::bind_front_handler(&LiveStateSession::on_write, shared_from_this()));
    }
}

void LiveStateSession::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        spdlog::warn("LiveStateSession write error: {}", ec.message());
        return; // Connection will be closed
    }

    queue_.erase(queue_.begin());

    if (!queue_.empty()) {
        ws_.async_write(
            net::buffer(*queue_.front()),
            beast::bind_front_handler(&LiveStateSession::on_write, shared_from_this()));
    } else {
        writing_ = false;
    }
}

// ---------------------------------------------------------

LiveStateServer::LiveStateServer(net::io_context& ioc, uint16_t port)
    : ioc_(ioc),
      acceptor_(ioc, tcp::endpoint(net::ip::make_address("127.0.0.1"), port)) {
}

void LiveStateServer::start() {
    spdlog::info("LiveStateServer: Starting WebSocket server on 127.0.0.1:{}", acceptor_.local_endpoint().port());
    do_accept();
}

void LiveStateServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&LiveStateServer::on_accept, shared_from_this()));
}

void LiveStateServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        spdlog::error("LiveStateServer accept error: {}", ec.message());
    } else {
        std::make_shared<LiveStateSession>(std::move(socket), *this)->run();
    }
    
    // Accept another connection
    do_accept();
}

void LiveStateServer::add_session(std::shared_ptr<LiveStateSession> session) {
    sessions_.insert(session.get());
    spdlog::info("LiveStateServer: Client connected. Total clients: {}", sessions_.size());
}

void LiveStateServer::remove_session(LiveStateSession* session) {
    sessions_.erase(session);
    spdlog::info("LiveStateServer: Client disconnected. Total clients: {}", sessions_.size());
}

void LiveStateServer::broadcast_state(const std::string& state_json) {
    if (sessions_.empty()) return;

    auto msg = std::make_shared<std::string>(state_json);
    
    for (auto* session : sessions_) {
        session->send_message(msg);
    }
}

} // namespace control
} // namespace trading
