#include "OrderRouter.h"
#include <boost/json.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <spdlog/spdlog.h>
#include <fmt/core.h>
#include <chrono>
#include <random>

namespace trading {
namespace exec {

OrderRouter::OrderRouter(boost::asio::io_context& ioc, 
                        boost::asio::ssl::context& ctx,
                        trading::StateStore& store,
                        risk::RiskManager& risk_manager,
                        const std::string& clob_api_url, 
                        const std::string& chain_id_str,
                        const std::string& verifying_contract,
                        const std::string& private_key_hex,
                        const std::string& funder_address,
                        bool paper_mode)
    : ioc_(ioc), ctx_(ctx), store_(store), risk_manager_(risk_manager),
      clob_api_url_(clob_api_url), funder_address_(funder_address), 
      paper_mode_(paper_mode) {
    signer_ = std::make_unique<EIP712Signer>(std::stoull(chain_id_str), verifying_contract, private_key_hex);
}

OrderRouter::~OrderRouter() {}

void OrderRouter::submit_order(const std::string& token_id, double price, double size, uint8_t side) {
    Order order;
    order.salt = generate_salt();
    order.maker = funder_address_;
    order.signer = funder_address_;
    order.taker = "0x0000000000000000000000000000000000000000";
    order.tokenId = token_id;
    
    uint64_t scale = 1000000;
    if (side == 0) { // BUY
        order.makerAmount = std::to_string(static_cast<uint64_t>(size * price * scale));
        order.takerAmount = std::to_string(static_cast<uint64_t>(size * scale));
    } else { // SELL
        order.makerAmount = std::to_string(static_cast<uint64_t>(size * scale));
        order.takerAmount = std::to_string(static_cast<uint64_t>(size * price * scale));
    }

    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(60);
    order.expiration = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count());
    
    order.nonce = "0"; 
    order.feeRateBps = "0"; 
    order.side = side; 
    order.signatureType = 0; 

    try {
        Signature sig = signer_->sign_order(order);
        if (paper_mode_) {
            simulate_paper_order(order, sig, "", "", 0.0, "MANUAL");
        } else {
            execute_rest_order(order, sig, "", "", 0.0, "MANUAL");
        }
    } catch (const std::exception& e) {
        spdlog::error("Order signature failed: {}", e.what());
    }
}

void OrderRouter::submit_latency_arb_order(const LatencyArbSignal& signal, double size_shares) {
    Order order;
    order.salt = generate_salt();
    order.maker = funder_address_;
    order.signer = funder_address_;
    order.taker = "0x0000000000000000000000000000000000000000";
    order.tokenId = signal.token_id;
    
    uint64_t scale = 1000000;
    order.makerAmount = std::to_string(static_cast<uint64_t>(size_shares * signal.polymarket_price * scale));
    order.takerAmount = std::to_string(static_cast<uint64_t>(size_shares * scale));

    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(60);
    order.expiration = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count());
    
    order.nonce = "0"; 
    order.feeRateBps = "0"; 
    order.side = 0; // BUY
    order.signatureType = 0; 

    try {
        Signature sig = signer_->sign_order(order);
        if (paper_mode_) {
            simulate_paper_order(order, sig, signal.asset, signal.market.question, signal.market.end_date_ts, "LA");
        } else {
            execute_rest_order(order, sig, signal.asset, signal.market.question, signal.market.end_date_ts, "LA");
        }
    } catch (const std::exception& e) {
        spdlog::error("Order signature failed: {}", e.what());
    }
}

void OrderRouter::submit_dump_hedge_order(const DumpHedgeSignal& signal, double size_shares) {
    std::string dh_id = "DH-" + signal.asset + "-" + std::to_string(static_cast<uint64_t>(signal.timestamp));
    
    if (paper_mode_) {
        risk::DumpHedgePosition dh_pos;
        dh_pos.dh_id = dh_id;
        dh_pos.asset = signal.asset;
        dh_pos.market_question = signal.market.question;
        dh_pos.yes_token_id = signal.yes_token_id;
        dh_pos.no_token_id = signal.no_token_id;
        dh_pos.yes_entry_price = signal.yes_price;
        dh_pos.no_entry_price = signal.no_price;
        dh_pos.combined_entry_price = signal.combined_price;
        dh_pos.size_shares = size_shares;
        dh_pos.combined_cost_usdc = signal.combined_price * size_shares;
        dh_pos.locked_profit_usdc = (1.0 - signal.combined_price) * size_shares;
        dh_pos.opened_at = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        dh_pos.end_date_ts = signal.market.end_date_ts;
        dh_pos.paper_mode = true;

        risk_manager_.register_dh_open(dh_pos);
        spdlog::info("[PAPER DH] OPENED | {} | Entry: {:.4f} | Locked Profit: ${:.2f}", signal.asset, signal.combined_price, dh_pos.locked_profit_usdc);
    } else {
        spdlog::info("[LIVE DH] Initiating dual-leg submission for {}...", signal.asset);
        
        // 1. Submit YES leg
        submit_order(signal.yes_token_id, signal.yes_price, size_shares, 0); // BUY
        
        // 2. Submit NO leg
        submit_order(signal.no_token_id, signal.no_price, size_shares, 0); // BUY

        // Register in RiskManager as a combined position
        risk::DumpHedgePosition dh_pos;
        dh_pos.dh_id = dh_id;
        dh_pos.asset = signal.asset;
        dh_pos.market_question = signal.market.question;
        dh_pos.yes_token_id = signal.yes_token_id;
        dh_pos.no_token_id = signal.no_token_id;
        dh_pos.yes_entry_price = signal.yes_price;
        dh_pos.no_entry_price = signal.no_price;
        dh_pos.combined_entry_price = signal.combined_price;
        dh_pos.size_shares = size_shares;
        dh_pos.combined_cost_usdc = signal.combined_price * size_shares;
        dh_pos.locked_profit_usdc = (1.0 - signal.combined_price) * size_shares;
        dh_pos.opened_at = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        dh_pos.end_date_ts = signal.market.end_date_ts;
        dh_pos.paper_mode = false;

        risk_manager_.register_dh_open(dh_pos);
        spdlog::info("[LIVE DH] REGISTERED | {} | Total Cost: ${:.2f}", signal.asset, dh_pos.combined_cost_usdc);
    }
}

void OrderRouter::submit_close_order(const std::string& order_id, const std::string& token_id, double current_price, double size, const std::string& asset, const std::string& question, double end_date_ts, const std::string& strategy) {
    Order order;
    order.salt = generate_salt();
    order.maker = funder_address_;
    order.signer = funder_address_;
    order.taker = "0x0000000000000000000000000000000000000000";
    order.tokenId = token_id;
    
    uint64_t scale = 1000000;
    order.makerAmount = std::to_string(static_cast<uint64_t>(size * scale));
    order.takerAmount = std::to_string(static_cast<uint64_t>(size * current_price * scale));

    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(60);
    order.expiration = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count());
    
    order.nonce = "0"; 
    order.feeRateBps = "0"; 
    order.side = 1; // SELL
    order.signatureType = 0; 

    try {
        Signature sig = signer_->sign_order(order);
        if (paper_mode_) {
            simulate_paper_order(order, sig, asset, question, end_date_ts, strategy, order_id);
        } else {
            execute_rest_order(order, sig, asset, question, end_date_ts, strategy, order_id);
        }
    } catch (const std::exception& e) {
        spdlog::error("Close order signature failed: {}", e.what());
    }
}

void OrderRouter::simulate_paper_order(const Order& order, const Signature& sig, const std::string& asset, const std::string& question, double end_date_ts, const std::string& strategy, const std::string& original_order_id) {
    if (order.side == 0) { // BUY
        double price = std::stod(order.makerAmount) / std::stod(order.takerAmount);
        double size_shares = std::stod(order.takerAmount) / 1000000.0;
        double cost = price * size_shares;

        risk::Position pos;
        pos.order_id = "paper_" + order.salt;
        pos.token_id = order.tokenId;
        pos.market_question = question;
        pos.side = "BUY";
        pos.entry_price = price;
        pos.size_shares = size_shares;
        pos.cost_usdc = cost;
        pos.opened_at = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        pos.end_date_ts = end_date_ts;
        pos.asset = asset;
        pos.strategy = strategy;
        pos.paper_mode = true;

        risk_manager_.register_trade_open(pos);
        spdlog::info("[PAPER TRADE] FILLED | {} | {} | Strategy: {} | Price: {:.4f} | Size: {:.2f} | Cost: ${:.2f}",
                     asset, question, strategy, price, size_shares, cost);
    } else { // SELL
        double price = std::stod(order.takerAmount) / std::stod(order.makerAmount);
        risk_manager_.register_trade_close(original_order_id, price);
        spdlog::info("[PAPER TRADE] CLOSED | {} | {} | Strategy: {} | Exit Price: {:.4f}",
                     asset, question, strategy, price);
    }
}

void OrderRouter::execute_rest_order(const Order& order, const Signature& sig, const std::string& asset, const std::string& question, double end_date_ts, const std::string& strategy, const std::string& original_order_id) {
    namespace beast = boost::beast;
    namespace http = beast::http;

    boost::json::object root;
    boost::json::object ord;
    ord["salt"] = order.salt;
    ord["maker"] = order.maker;
    ord["signer"] = order.signer;
    ord["taker"] = order.taker;
    ord["tokenId"] = order.tokenId;
    ord["makerAmount"] = order.makerAmount;
    ord["takerAmount"] = order.takerAmount;
    ord["expiration"] = order.expiration;
    ord["nonce"] = order.nonce;
    ord["feeRateBps"] = order.feeRateBps;
    ord["side"] = order.side;
    ord["signatureType"] = order.signatureType;
    root["order"] = std::move(ord);
    root["signature"] = sig.rsv_hex;
    root["owner"] = funder_address_;

    std::string payload = boost::json::serialize(root);
    
    spdlog::info("[LIVE EXEC] Dispatching order to Polymarket CLOB: {}", payload);

    try {
        // Simple synchronous POST via Beast
        std::string host = "clob.polymarket.com";
        std::string target = "/orders";

        boost::asio::ip::tcp::resolver resolver(ioc_);
        beast::ssl_stream<beast::tcp_stream> stream(ioc_, ctx_);

        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw std::runtime_error("Failed to set SNI hostname");
        }

        auto const results = resolver.resolve(host, "443");
        beast::get_lowest_layer(stream).connect(results);
        stream.handshake(boost::asio::ssl::stream_base::client);

        http::request<http::string_body> req{http::verb::post, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "PolymarketBot/1.0");
        req.set(http::field::content_type, "application/json");
        req.body() = payload;
        req.prepare_payload();

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        double target_price = 0.0;
        double size_shares = 0.0;
        if (order.side == 0) { // BUY
            target_price = std::stod(order.makerAmount) / std::stod(order.takerAmount);
            size_shares = std::stod(order.takerAmount) / 1000000.0;
        } else { // SELL
            target_price = std::stod(order.takerAmount) / std::stod(order.makerAmount);
            size_shares = std::stod(order.makerAmount) / 1000000.0;
        }

        beast::error_code ec;
        stream.shutdown(ec);

        if (res.result() != http::status::ok && res.result() != http::status::created) {
            spdlog::error("[LIVE EXEC] Order REJECTED by CLOB: {} | Body: {}", res.result_int(), res.body());
            return;
        }

        auto response_json = boost::json::parse(res.body()).as_object();
        spdlog::info("[LIVE EXEC] Order Response: {}", res.body());

        double actual_price = target_price;
        double filled_size = 0.0;

        if (response_json.contains("price")) {
            actual_price = std::stod(std::string(response_json["price"].as_string()));
        }
        if (response_json.contains("size_matched")) {
            filled_size = std::stod(std::string(response_json["size_matched"].as_string())) / 1000000.0;
        } else if (response_json.contains("status") && response_json["status"].as_string() == "filled") {
            filled_size = size_shares; // Fallback if status is filled
        }

        if (filled_size <= 0) {
            spdlog::warn("[LIVE EXEC] Order accepted by CLOB but 0 size matched. No position opened.");
            return;
        }

        if (order.side == 0) { // BUY
            double slippage = (actual_price - target_price) / target_price;
            
            // Register position with ACTUAL data
            risk::Position pos;
            pos.order_id = "live_" + order.salt;
            pos.token_id = order.tokenId;
            pos.market_question = question;
            pos.side = "BUY";
            pos.entry_price = actual_price; 
            pos.size_shares = filled_size;
            pos.cost_usdc = actual_price * filled_size;
            pos.opened_at = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            pos.end_date_ts = end_date_ts;
            pos.asset = asset;
            pos.strategy = strategy;
            pos.paper_mode = false;

            risk_manager_.register_trade_open(pos);
            spdlog::info("[LIVE EXEC] Trade FILLED | {} | Price: {:.4f} | Size: {:.2f} | Slippage: {:.4f}%", 
                         asset, actual_price, filled_size, slippage * 100.0);
        } else { // SELL
            risk_manager_.register_trade_close(original_order_id, actual_price);
            spdlog::info("[LIVE EXEC] Trade CLOSED | {} | Price: {:.4f}", asset, actual_price);
        }
    } catch (const std::exception& e) {
        spdlog::error("[LIVE EXEC] Network error during order submission: {}", e.what());
    }
}

std::string OrderRouter::generate_salt() const {
    static std::mt19937_64 gen(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dis;
    return std::to_string(dis(gen));
}

} // namespace exec
} // namespace trading
