#include <iostream>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "state/StateStore.h"
#include "feeds/BinanceFeed.h"
#include "feeds/PolymarketFeed.h"
#include "exec/OrderRouter.h"
#include "risk/RiskManager.h"
#include <fstream>
#include <map>
#include <string>

std::map<std::string, std::string> load_env() {
    std::map<std::string, std::string> env;
    std::ifstream file(".env");
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            if (!value.empty() && value.back() == '\r') value.pop_back();
            env[key] = value;
        }
    }
    return env;
}

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting Polymarket Arbitrage Trading Core");

    try {
        boost::asio::io_context ioc;
        boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
        
        // Use default root certs
        ctx.set_default_verify_paths();
        // In production, we should strictly verify the peer, but for this dev stage we skip
        ctx.set_verify_mode(boost::asio::ssl::verify_none);

        trading::StateStore store;
        
        // Start Binance
        auto binance = std::make_shared<trading::BinanceFeed>(ioc, ctx, store, "btcusdt");
        binance->start();

        // Start Polymarket
        auto polymarket = std::make_shared<trading::PolymarketFeed>(ioc, ctx, store);
        polymarket->start();
        
        // In the real system, the Control Plane or Model would discover active token IDs
        // and call subscribe on the polymarket feed. For now, we connect and wait.

        // Setup signals to stop the io_context cleanly
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int signal_number) {
            spdlog::info("Received signal {}, shutting down...", signal_number);
            binance->stop();
            polymarket->stop();
            ioc.stop();
        });

        auto env = load_env();
        bool paper_mode = (env["PAPER_MODE"] == "true");
        std::string chain_id = env["POLYMARKET_CHAIN_ID"].empty() ? "137" : env["POLYMARKET_CHAIN_ID"];
        std::string host = env["POLYMARKET_HOST"].empty() ? "https://clob.polymarket.com" : env["POLYMARKET_HOST"];
        std::string verifying_contract = "0x4bFB41d5B3570DeFd03C39a9A4D8dE6Bd8B8982E";
        
        // Load RiskManager limits from .env
        auto parse_env_double = [&](const std::string& key, double default_val) {
            return env[key].empty() ? default_val : std::stod(env[key]);
        };
        auto parse_env_int = [&](const std::string& key, int default_val) {
            return env[key].empty() ? default_val : std::stoi(env[key]);
        };

        double starting_balance = parse_env_double("PAPER_STARTING_BALANCE", 1000.0);
        double max_position_fraction = parse_env_double("RISK_MAX_POSITION_FRACTION", 0.08);
        double daily_loss_limit = parse_env_double("RISK_DAILY_LOSS_LIMIT", 0.20);
        double total_drawdown_kill = parse_env_double("RISK_TOTAL_DRAWDOWN_KILL", 0.40);
        int max_concurrent_positions = parse_env_int("RISK_MAX_CONCURRENT_POSITIONS", 3);

        auto risk_manager = std::make_shared<risk::RiskManager>(
            starting_balance,
            max_position_fraction,
            daily_loss_limit,
            total_drawdown_kill,
            max_concurrent_positions
            // Circuit breaker defaults are fine for now
        );
        
        auto router = std::make_shared<trading::exec::OrderRouter>(
            ioc, host, chain_id, verifying_contract, 
            env["POLYMARKET_PRIVATE_KEY"], env["POLYMARKET_FUNDER"], paper_mode
        );

        // Test EIP-712 execution locally via a timer
        auto test_timer = std::make_shared<boost::asio::steady_timer>(ioc, std::chrono::seconds(5));
        test_timer->async_wait([router](const boost::system::error_code& ec) {
            if (!ec) {
                spdlog::info("Dispatching test order to OrderRouter...");
                // Note: Token ID must be a valid uint256 hex string
                router->submit_order("0x0000000000000000000000000000000000000000000000000000000000000123", 0.45, 10.0, 0); // BUY 10 shares at 0.45
            }
        });

        spdlog::info("Initialization complete. Running event loop.");
        ioc.run();
        
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Trading Core stopped gracefully.");
    return EXIT_SUCCESS;
}
