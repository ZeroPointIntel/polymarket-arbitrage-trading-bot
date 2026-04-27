#include <iostream>
#include <spdlog/spdlog.h>
#include <boost/asio.h>
#include <boost/asio/ssl.hpp>
#include "state/StateStore.h"
#include "feeds/BinanceFeed.h"
#include "feeds/PolymarketFeed.h"

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

        spdlog::info("Initialization complete. Running event loop.");
        ioc.run();
        
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Trading Core stopped gracefully.");
    return EXIT_SUCCESS;
}
