#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <string>
#include <unordered_set>

namespace trading {
namespace control {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class LiveStateSession : public std::enable_shared_from_this<LiveStateSession> {
public:
    explicit LiveStateSession(tcp::socket&& socket, class LiveStateServer& server);
    ~LiveStateSession();

    void run();
    void send_message(std::shared_ptr<std::string> msg);

private:
    void on_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);

    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    class LiveStateServer& server_;
    std::vector<std::shared_ptr<std::string>> queue_;
    bool writing_ = false;
};

class LiveStateServer : public std::enable_shared_from_this<LiveStateServer> {
public:
    LiveStateServer(net::io_context& ioc, uint16_t port);
    
    void start();
    void broadcast_state(const std::string& state_json);

    void add_session(std::shared_ptr<LiveStateSession> session);
    void remove_session(LiveStateSession* session);

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::unordered_set<LiveStateSession*> sessions_;
};

} // namespace control
} // namespace trading
