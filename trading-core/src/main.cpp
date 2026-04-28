#include <iostream>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "state/StateStore.h"
#include "feeds/BinanceFeed.h"
#include "feeds/PolymarketFeed.h"
#include "feeds/GammaClient.h"
#include "exec/OrderRouter.h"
#include "risk/RiskManager.h"
#include "signals/SignalEngine.h"
#include "control/LiveStateServer.h"
#include "model/FairValueModel.h"
#include <boost/json.hpp>
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

void run_evaluation_loop(boost::asio::steady_timer& timer, 
                         trading::SignalEngine& engine,
                         std::shared_ptr<trading::control::LiveStateServer> live_server,
                         std::shared_ptr<risk::RiskManager> risk_manager,
                         trading::StateStore& store,
                         const trading::MarketInfo& active_market) {
    timer.expires_after(std::chrono::milliseconds(50));
    timer.async_wait([&, live_server, risk_manager, active_market](const boost::system::error_code& ec) {
        if (!ec) {
            double current_time_ms = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
            engine.tick(current_time_ms);
            
            // Broadcast live state to Next.js
            boost::json::object state;
            state["balance"] = risk_manager->get_current_balance();
            state["winRate"] = risk_manager->get_win_rate();
            state["openPositions"] = risk_manager->get_open_position_count();
            state["status"] = static_cast<int>(risk_manager->get_status());
            
            auto btc_tick = store.get_latest_btc_price();
            double btc_price = btc_tick ? btc_tick->price : 0.0;
            state["btcPrice"] = btc_price;
            
            auto pm_tick = store.get_token_price(active_market.yes_token_id);
            double pm_price = pm_tick ? pm_tick->price : 0.0;
            state["polymarketPrice"] = pm_price;
            
            double seconds_remaining = active_market.end_date_ts - (current_time_ms / 1000.0);
            double fair_value = 0.5;
            if (btc_price > 0 && seconds_remaining > 0) {
                fair_value = model::FairValueModel::compute_fair_value_5m(
                    btc_price, active_market.strike, seconds_remaining, "UP", 150.0, 20.0);
            }
            state["fairValue"] = fair_value;
            state["timestamp"] = current_time_ms;
            
            live_server->broadcast_state(boost::json::serialize(state));
            
            run_evaluation_loop(timer, engine, live_server, risk_manager, store, active_market);
        }
    });
}

void run_heartbeat(boost::asio::steady_timer& timer, std::shared_ptr<risk::RiskManager> risk_manager) {
    timer.expires_after(std::chrono::seconds(10));
    timer.async_wait([&, risk_manager](const boost::system::error_code& ec) {
        if (!ec) {
            spdlog::info("[HEARTBEAT] Status: {} | Balance: ${:.2f} | Open Positions: {} | Win Rate: {:.1f}%",
                         static_cast<int>(risk_manager->get_status()),
                         risk_manager->get_current_balance(),
                         risk_manager->get_open_position_count(),
                         risk_manager->get_win_rate() * 100.0);
            run_heartbeat(timer, risk_manager);
        }
    });
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Starting Polymarket Arbitrage Trading Core - Phase 5");

    try {
        boost::asio::io_context ioc;
        boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
        
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(boost::asio::ssl::verify_none);

        auto env = load_env();
        bool paper_mode = (env["PAPER_MODE"] == "true" || env["PAPER_MODE"].empty()); // Default to true
        std::string chain_id = env["POLYMARKET_CHAIN_ID"].empty() ? "137" : env["POLYMARKET_CHAIN_ID"];
        std::string host = env["POLYMARKET_HOST"].empty() ? "https://clob.polymarket.com" : env["POLYMARKET_HOST"];
        std::string verifying_contract = "0x4bFB41d5B3570DeFd03C39a9A4D8dE6Bd8B8982E";
        
        trading::StateStore store;
        
        // 1. Fetch Active Market via Gamma
        spdlog::info("Fetching active btc-updown-5m market from Gamma API...");
        trading::GammaClient gamma(ioc, ctx);
        auto market_opt = gamma.fetch_active_btc_market();
        if (!market_opt) {
            spdlog::critical("Failed to find active BTC market. Cannot continue.");
            return EXIT_FAILURE;
        }
        trading::MarketInfo active_market = *market_opt;
        spdlog::info("Discovered Market: {}", active_market.question);
        spdlog::info("YES Token: {}", active_market.yes_token_id);
        spdlog::info("NO Token: {}", active_market.no_token_id);
        std::vector<trading::MarketInfo> active_markets = { active_market };

        // 2. Start Binance Feed
        auto binance = std::make_shared<trading::BinanceFeed>(ioc, ctx, store, "btcusdt");
        binance->start();

        // 3. Start Polymarket Feed and Subscribe to the discovered tokens
        auto polymarket = std::make_shared<trading::PolymarketFeed>(ioc, ctx, store);
        polymarket->start();
        
        std::vector<std::string> target_tokens = { active_market.yes_token_id, active_market.no_token_id };
        polymarket->subscribe(target_tokens);

        // 4. Initialize RiskManager
        auto parse_env_double = [&](const std::string& key, double default_val) {
            return env[key].empty() ? default_val : std::stod(env[key]);
        };
        auto parse_env_int = [&](const std::string& key, int default_val) {
            return env[key].empty() ? default_val : std::stoi(env[key]);
        };

        double starting_balance = parse_env_double("PAPER_STARTING_BALANCE", 1000.0);
        auto risk_manager = std::make_shared<risk::RiskManager>(
            starting_balance,
            parse_env_double("RISK_MAX_POSITION_FRACTION", 0.08),
            parse_env_double("RISK_DAILY_LOSS_LIMIT", 0.20),
            parse_env_double("RISK_TOTAL_DRAWDOWN_KILL", 0.40),
            parse_env_int("RISK_MAX_CONCURRENT_POSITIONS", 3)
        );
        
        // 5. Initialize OrderRouter
        auto router = std::make_shared<trading::exec::OrderRouter>(
            ioc, host, chain_id, verifying_contract, 
            env["POLYMARKET_PRIVATE_KEY"], env["POLYMARKET_FUNDER"], paper_mode
        );

        // 6. Initialize SignalEngine
        trading::SignalEngine engine(store, risk_manager, router, active_markets);

        // 7. Start the Live State WS Server
        auto live_server = std::make_shared<trading::control::LiveStateServer>(ioc, 8080);
        live_server->start();

        // 8. Start the evaluation loop (50ms tick)
        boost::asio::steady_timer eval_timer(ioc);
        run_evaluation_loop(eval_timer, engine, live_server, risk_manager, store, active_market);

        // 8. Start the heartbeat timer (10s)
        boost::asio::steady_timer hb_timer(ioc);
        run_heartbeat(hb_timer, risk_manager);

        // Handle signals to stop the io_context cleanly
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int signal_number) {
            spdlog::info("Received signal {}, shutting down...", signal_number);
            eval_timer.cancel();
            hb_timer.cancel();
            binance->stop();
            polymarket->stop();
            ioc.stop();
        });

        spdlog::info("Initialization complete. Running live event loop.");
        ioc.run();
        
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Trading Core stopped gracefully.");
    return EXIT_SUCCESS;
}
