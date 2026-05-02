#pragma once

#include "EIP712Signer.h"
#include <string>
#include <memory>
#include <boost/asio.hpp>

namespace trading {
namespace exec {

class OrderRouter {
public:
    OrderRouter(boost::asio::io_context& ioc, 
                const std::string& clob_api_url, 
                const std::string& chain_id_str,
                const std::string& verifying_contract,
                const std::string& private_key_hex,
                const std::string& funder_address,
                bool paper_mode);

    ~OrderRouter();

    // Sends an order. If paper_mode_ is true, it simulates the execution instead.
    void submit_order(const std::string& token_id, 
                      double price, 
                      double size, 
                      uint8_t side);

private:
    std::string clob_api_url_;
    std::string funder_address_;
    bool paper_mode_;
    
    std::unique_ptr<EIP712Signer> signer_;

    void execute_rest_order(const Order& order, const Signature& sig);
    void simulate_paper_order(const Order& order, const Signature& sig);

    std::string generate_salt() const;
};

} // namespace exec
} // namespace trading
