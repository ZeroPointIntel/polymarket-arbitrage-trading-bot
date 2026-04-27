#include "OrderRouter.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/json.hpp>
#include <spdlog/spdlog.h>
#include <random>
#include <chrono>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace trading {
namespace exec {

OrderRouter::OrderRouter(boost::asio::io_context& ioc, 
                         const std::string& clob_api_url, 
                         const std::string& chain_id_str,
                         const std::string& verifying_contract,
                         const std::string& private_key_hex,
                         const std::string& funder_address,
                         bool paper_mode)
    : ioc_(ioc), 
      clob_api_url_(clob_api_url), 
      funder_address_(funder_address),
      paper_mode_(paper_mode) {
    
    uint64_t chain_id = std::stoull(chain_id_str);
    signer_ = std::make_unique<EIP712Signer>(chain_id, verifying_contract, private_key_hex);
    
    if (paper_mode_) {
        spdlog::info("OrderRouter initialized in PAPER MODE. No real orders will be placed.");
    } else {
        spdlog::info("OrderRouter initialized in LIVE MODE.");
    }
}

OrderRouter::~OrderRouter() {}

std::string OrderRouter::generate_salt() const {
    // Generate a random 256-bit salt for the order
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t p1 = dis(gen);
    uint64_t p2 = dis(gen);
    // Convert to a big number string (simplified approach for now)
    return std::to_string(p1) + std::to_string(p2);
}

void OrderRouter::submit_order(const std::string& token_id, double price, double size, uint8_t side) {
    Order order;
    order.salt = generate_salt();
    order.maker = funder_address_;
    order.signer = funder_address_;
    order.taker = "0x0000000000000000000000000000000000000000";
    order.tokenId = token_id;
    
    // makerAmount / takerAmount calculation depends on side (BUY vs SELL)
    // For Polymarket:
    // BUY: makerAmount = size * price, takerAmount = size
    // SELL: makerAmount = size, takerAmount = size * price
    // Amounts must be scaled to 6 decimals (USDC/conditional tokens usually have 6 decimals)
    uint64_t scale = 1000000;
    if (side == 0) { // BUY
        order.makerAmount = std::to_string(static_cast<uint64_t>(size * price * scale));
        order.takerAmount = std::to_string(static_cast<uint64_t>(size * scale));
    } else { // SELL
        order.makerAmount = std::to_string(static_cast<uint64_t>(size * scale));
        order.takerAmount = std::to_string(static_cast<uint64_t>(size * price * scale));
    }

    // Expiration: current time + 60 seconds
    auto now = std::chrono::system_clock::now();
    auto exp = now + std::chrono::seconds(60);
    order.expiration = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(exp.time_since_epoch()).count());
    
    order.nonce = "0"; // Handled by API
    order.feeRateBps = "0"; // Default
    order.side = side;
    order.signatureType = 0; // EOA

    try {
        Signature sig = signer_->sign_order(order);
        
        if (paper_mode_) {
            simulate_paper_order(order, sig);
        } else {
            execute_rest_order(order, sig);
        }
    } catch (const std::exception& e) {
        spdlog::error("Order signature failed: {}", e.what());
    }
}

void OrderRouter::simulate_paper_order(const Order& order, const Signature& sig) {
    spdlog::info("[PAPER TRADE] Order simulated successfully!");
    spdlog::info("  Side: {}", order.side == 0 ? "BUY" : "SELL");
    spdlog::info("  Token: {}", order.tokenId);
    spdlog::info("  MakerAmount: {}", order.makerAmount);
    spdlog::info("  TakerAmount: {}", order.takerAmount);
    spdlog::info("  Signed RSV Payload: {}", sig.rsv_hex);
}

void OrderRouter::execute_rest_order(const Order& order, const Signature& sig) {
    // This connects to the CLOB REST API and submits a POST /order
    // (Implementation of beast HTTP POST goes here, abstracted for brevity)
    spdlog::info("Executing live REST order to POST /order...");
    
    boost::json::object payload;
    payload["order"] = boost::json::object{
        {"salt", order.salt},
        {"maker", order.maker},
        {"signer", order.signer},
        {"taker", order.taker},
        {"tokenId", order.tokenId},
        {"makerAmount", order.makerAmount},
        {"takerAmount", order.takerAmount},
        {"expiration", order.expiration},
        {"nonce", order.nonce},
        {"feeRateBps", order.feeRateBps},
        {"side", order.side == 0 ? "BUY" : "SELL"},
        {"signatureType", order.signatureType}
    };
    payload["signature"] = sig.rsv_hex;

    std::string json_str = boost::json::serialize(payload);
    spdlog::debug("Payload: {}", json_str);
    
    // Actual Boost.Beast execution logic can be attached here in phase 3, 
    // or run asynchronously on the ioc_
}

} // namespace exec
} // namespace trading
