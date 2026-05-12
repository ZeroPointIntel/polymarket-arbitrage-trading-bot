#pragma once

#include "EIP712Signer.h"
#include "../state/StateStore.h"
#include "../risk/RiskManager.h"
#include "../signals/Signal.h"
#include <string>
#include <memory>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace trading {
namespace exec {

class OrderRouter {
public:
    OrderRouter(boost::asio::io_context& ioc, 
                boost::asio::ssl::context& ctx,
                trading::StateStore& store,
                risk::RiskManager& risk_manager,
                const std::string& clob_api_url, 
                const std::string& chain_id_str,
                const std::string& verifying_contract,
                const std::string& private_key_hex,
                const std::string& funder_address,
                bool paper_mode,
                const std::string& api_key = "",
                const std::string& api_secret = "",
                const std::string& api_passphrase = "");

    ~OrderRouter();

    void submit_order(const std::string& token_id, 
                      double price, 
                      double size, 
                      uint8_t side);

    void submit_latency_arb_order(const LatencyArbSignal& signal, double size);
    void submit_dump_hedge_order(const DumpHedgeSignal& signal, double size);
    void submit_close_order(const std::string& order_id, const std::string& token_id, double current_price, double size, const std::string& asset, const std::string& question, double end_date_ts, const std::string& strategy);

private:
    boost::asio::io_context& ioc_;
    boost::asio::ssl::context& ctx_;
    trading::StateStore& store_;
    risk::RiskManager& risk_manager_;

    std::string clob_api_url_;
    std::string funder_address_;
    bool paper_mode_;
    std::string api_key_;
    std::string api_secret_;
    std::string api_passphrase_;
    
    std::unique_ptr<EIP712Signer> signer_;

    void execute_rest_order(const Order& order, const Signature& sig, const std::string& asset = "", const std::string& question = "", double end_date_ts = 0.0, const std::string& strategy = "LA", const std::string& original_order_id = "");
    void simulate_paper_order(const Order& order, const Signature& sig, const std::string& asset = "", const std::string& question = "", double end_date_ts = 0.0, const std::string& strategy = "LA", const std::string& original_order_id = "");

    std::string generate_salt() const;
    std::string compute_hmac_signature(const std::string& timestamp, const std::string& method, const std::string& path, const std::string& body);
    std::string base64_encode(const unsigned char* input, int length);
    std::vector<unsigned char> base64_decode(const std::string& input);
    int calc_decode_length(const std::string& b64input);
};

} // namespace exec
} // namespace trading
