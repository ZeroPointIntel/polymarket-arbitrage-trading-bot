#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <thread>
#include <chrono>
#include <mutex>
#include <algorithm>
#include "state/StateStore.h"
#include "feeds/BinanceFeed.h"
#include "feeds/PolymarketFeed.h"
#include "feeds/GammaClient.h"
#include "risk/RiskManager.h"
#include "risk/KellySizer.h"
#include "signals/LatencyArbDetector.h"
#include "signals/DumpHedgeDetector.h"
#include "exec/OrderRouter.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <fmt/core.h>
#include <atomic>

using namespace trading;

struct ExitConfig {
    double near_win_price = 0.92;
    double near_loss_price = 0.08;
    double take_profit_price = 0.72;
    double take_profit_pnl = 0.15;
    double stop_loss_pnl = -0.18;
    double position_timeout_seconds = 270.0; // 4.5 minutes
    double trailing_stop_activation = 0.06;
    double trailing_stop_distance = 0.04;
};

std::unordered_map<std::string, std::string> load_env(const std::string& filepath) {
    std::unordered_map<std::string, std::string> env;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::warn("Could not open {} - using defaults", filepath);
        return env;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }
            env[key] = val;
        }
    }
    return env;
}

void check_and_close_positions(risk::RiskManager& risk_manager, StateStore& store, exec::OrderRouter& router, const ExitConfig& cfg) {
    auto now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    // 1. Process Latency Arb (LA) positions - with Early Exit logic
    auto open_la = risk_manager.get_open_positions();
    for (auto& [id, p] : open_la) {
        if (p.strategy != "LA") continue;

        auto live_bid = store.get_token_bid(p.token_id);
        double current_price = live_bid ? live_bid->price : 0.0;
        double current_pnl_pct = (p.entry_price > 0) ? (current_price - p.entry_price) / p.entry_price : 0.0;
        double age = now - p.opened_at;

        // Update peak price
        if (current_price > p.peak_price) {
            p.peak_price = current_price;
            risk_manager.update_peak_price(id, current_price);
        }
        
        double peak_pnl_pct = (p.entry_price > 0) ? (p.peak_price - p.entry_price) / p.entry_price : 0.0;
        double drawdown_from_peak = (p.peak_price > 0) ? (p.peak_price - current_price) / p.peak_price : 0.0;

        bool should_exit = false;
        std::string exit_reason = "";

        if (now >= p.end_date_ts) {
            should_exit = true;
            exit_reason = "EXPIRED";
        } else if (cfg.stop_loss_pnl < 0 && current_pnl_pct <= cfg.stop_loss_pnl) {
            should_exit = true;
            exit_reason = fmt::format("Stop loss: {:.1f}%", current_pnl_pct * 100.0);
        } else if (peak_pnl_pct >= cfg.trailing_stop_activation && drawdown_from_peak >= cfg.trailing_stop_distance) {
            should_exit = true;
            exit_reason = fmt::format("Trailing stop: peak {:.3f} -> {:.3f} (-{:.1f}%)", p.peak_price, current_price, drawdown_from_peak * 100.0);
        } else if (current_price >= cfg.near_win_price) {
            should_exit = true;
            exit_reason = fmt::format("Near resolution win: {:.3f}", current_price);
        } else if (current_price <= cfg.near_loss_price && current_price > 0) {
            should_exit = true;
            exit_reason = fmt::format("Near resolution loss: {:.3f}", current_price);
        } else if ((current_price >= cfg.take_profit_price && current_pnl_pct > 0) || current_pnl_pct >= cfg.take_profit_pnl) {
            should_exit = true;
            exit_reason = fmt::format("Take profit: {:.1f}%", current_pnl_pct * 100.0);
        } else if (age >= cfg.position_timeout_seconds) {
            should_exit = true;
            exit_reason = fmt::format("Timeout: {:.0f}s", age);
        }

        if (should_exit && current_price > 0) {
            if (now >= p.end_date_ts) {
                // Resolution at expiry (Polymarket settle)
                double exit_price = (current_price >= 0.5) ? 1.0 : 0.0;
                risk_manager.register_trade_close(id, exit_price);
                store.push_telemetry(fmt::format("SETTLED {} {} @ {:.2f} | {}", p.asset, exit_price >= 1.0 ? "WIN" : "LOSS", exit_price, p.market_question));
            } else {
                // Dynamic Early Exit - SELL to CLOB
                router.submit_close_order(id, p.token_id, current_price, p.size_shares, p.asset, p.market_question, p.end_date_ts, "LA");
                store.push_telemetry(fmt::format("LA EARLY EXIT {} | {} | PnL: {:.1f}%", p.asset, exit_reason, current_pnl_pct * 100.0));
            }
        }
    }

    // 2. Process Dump Hedge (DH) positions - only on expiry
    auto open_dh = risk_manager.get_open_dh_positions();
    for (const auto& [id, p] : open_dh) {
        if (now >= p.end_date_ts) {
            auto live_y = store.get_token_price(p.yes_token_id);
            auto live_n = store.get_token_price(p.no_token_id);
            double ey = live_y ? ((live_y->price >= 0.5) ? 1.0 : 0.0) : 0.5;
            double en = live_n ? ((live_n->price >= 0.5) ? 1.0 : 0.0) : 0.5;
            risk_manager.register_dh_close(id, ey, en, "EXPIRED");
            std::string dh_outcome = (ey >= 1.0 || en >= 1.0) ? "WIN" : "LOSS";
            store.push_telemetry(fmt::format("SETTLED {} {} @ {:.2f} | {}",
                p.asset, dh_outcome, std::max(ey, en), p.market_question));
        }
    }
}

int main() {
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("bot.log", true);
        auto logger = std::make_shared<spdlog::logger>("bot", file_sink);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        auto env = load_env(".env");
        bool paper_mode = true;
        if (env.count("PAPER_MODE")) {
            std::string pm = env["PAPER_MODE"];
            std::transform(pm.begin(), pm.end(), pm.begin(), ::tolower);
            if (pm == "false" || pm == "0") paper_mode = false;
        }
        
        double starting_balance = 1000.0;
        if (paper_mode && env.count("PAPER_STARTING_BALANCE")) {
            starting_balance = std::stod(env["PAPER_STARTING_BALANCE"]);
        } else if (!paper_mode && env.count("LIVE_STARTING_BALANCE")) {
            starting_balance = std::stod(env["LIVE_STARTING_BALANCE"]);
        }

        std::string polymarket_host = env.count("POLYMARKET_HOST") ? env["POLYMARKET_HOST"] : "https://clob.polymarket.com";
        std::string polymarket_chain_id = env.count("POLYMARKET_CHAIN_ID") ? env["POLYMARKET_CHAIN_ID"] : "137";
        std::string polymarket_funder = env.count("POLYMARKET_FUNDER") ? env["POLYMARKET_FUNDER"] : "0x0000000000000000000000000000000000000000";
        std::string polymarket_pk = env.count("POLYMARKET_PRIVATE_KEY") ? env["POLYMARKET_PRIVATE_KEY"] : "0x0000000000000000000000000000000000000000000000000000000000000001";
        std::string verifying_contract = env.count("POLYMARKET_EXCHANGE_ADDRESS") ? env["POLYMARKET_EXCHANGE_ADDRESS"] : "0x4bFb9eC6c905307De7724730991afD718ee99312";

        ExitConfig exit_cfg;
        if (env.count("NEAR_WIN_PRICE")) exit_cfg.near_win_price = std::stod(env["NEAR_WIN_PRICE"]);
        if (env.count("NEAR_LOSS_PRICE")) exit_cfg.near_loss_price = std::stod(env["NEAR_LOSS_PRICE"]);
        if (env.count("TAKE_PROFIT_PRICE")) exit_cfg.take_profit_price = std::stod(env["TAKE_PROFIT_PRICE"]);
        if (env.count("TAKE_PROFIT_PNL")) exit_cfg.take_profit_pnl = std::stod(env["TAKE_PROFIT_PNL"]);
        if (env.count("STOP_LOSS_PNL")) exit_cfg.stop_loss_pnl = std::stod(env["STOP_LOSS_PNL"]);
        if (env.count("POSITION_TIMEOUT_SECONDS")) exit_cfg.position_timeout_seconds = std::stod(env["POSITION_TIMEOUT_SECONDS"]);
        if (env.count("TRAILING_STOP_ACTIVATION")) exit_cfg.trailing_stop_activation = std::stod(env["TRAILING_STOP_ACTIVATION"]);
        if (env.count("TRAILING_STOP_DISTANCE")) exit_cfg.trailing_stop_distance = std::stod(env["TRAILING_STOP_DISTANCE"]);

        spdlog::info("Starting Core v2.2 | Mode: {} | Bal: ${:.2f}", paper_mode ? "PAPER" : "LIVE", starting_balance);

        boost::asio::io_context feed_ioc;
        boost::asio::ssl::context feed_ctx{boost::asio::ssl::context::sslv23_client};
        feed_ctx.set_default_verify_paths();

        boost::asio::io_context gamma_ioc;
        boost::asio::ssl::context gamma_ctx{boost::asio::ssl::context::sslv23_client};
        gamma_ctx.set_default_verify_paths();

        double max_pos = env.count("RISK_MAX_POSITION_FRACTION") ? std::stod(env["RISK_MAX_POSITION_FRACTION"]) : 0.08;
        double daily_loss = env.count("RISK_DAILY_LOSS_LIMIT") ? std::stod(env["RISK_DAILY_LOSS_LIMIT"]) : 0.20;
        double drawdown = env.count("RISK_TOTAL_DRAWDOWN_KILL") ? std::stod(env["RISK_TOTAL_DRAWDOWN_KILL"]) : 0.40;
        int max_concurrent = env.count("RISK_MAX_CONCURRENT_POSITIONS") ? std::stoi(env["RISK_MAX_CONCURRENT_POSITIONS"]) : 3;
        double min_order = env.count("MIN_ORDER_SIZE") ? std::stod(env["MIN_ORDER_SIZE"]) : 5.0;

        StateStore store;
        store.set_paper_mode(paper_mode);
        risk::RiskManager risk_manager(starting_balance, max_pos, daily_loss, drawdown, max_concurrent, true, 3, 5, 0.02, 300.0, min_order);
        store.set_risk_manager(&risk_manager);
        KellySizer kelly_sizer(0.5, 0.08);

        exec::OrderRouter router(feed_ioc, feed_ctx, store, risk_manager, polymarket_host, polymarket_chain_id, verifying_contract, polymarket_pk, polymarket_funder, paper_mode);

        GammaClient gamma(gamma_ioc, gamma_ctx);
        auto btc_feed = std::make_shared<BinanceFeed>(feed_ioc, feed_ctx, store, "btcusdt");
        auto eth_feed = std::make_shared<BinanceFeed>(feed_ioc, feed_ctx, store, "ethusdt");
        auto sol_feed = std::make_shared<BinanceFeed>(feed_ioc, feed_ctx, store, "solusdt");

        // Feeds will be started after callbacks are registered

        auto feed_work = boost::asio::make_work_guard(feed_ioc);
        std::thread feed_thread([&feed_ioc]() { feed_ioc.run(); });

        std::mutex detector_mutex;

        std::vector<std::unique_ptr<LatencyArbDetector>> la_detectors;
        auto price_resolver = [&gamma](const std::string& token_id, const std::string& side) { return gamma.fetch_token_price(token_id, side); };
        la_detectors.push_back(std::make_unique<LatencyArbDetector>(store, std::vector<MarketInfo>{}, 0.02, 60.0, 15.0, 2.7, "btc", price_resolver));
        la_detectors.push_back(std::make_unique<LatencyArbDetector>(store, std::vector<MarketInfo>{}, 0.02, 60.0, 15.0, 2.7, "eth", price_resolver));
        la_detectors.push_back(std::make_unique<LatencyArbDetector>(store, std::vector<MarketInfo>{}, 0.02, 60.0, 15.0, 2.7, "sol", price_resolver));

        std::unique_ptr<DumpHedgeDetector> dh_detector;
        auto eval_la_for_asset = [&](const std::string& sym) {
            std::lock_guard<std::mutex> lock(detector_mutex);
            double now_ms = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
            for (auto& det : la_detectors) {
                auto signal = det->evaluate(now_ms);
                if (signal) {
                    auto kelly = kelly_sizer.calculate(risk_manager.get_current_balance(), signal->fair_value, signal->polymarket_price);
                    if (kelly && risk_manager.can_open_position(kelly->position_size_usdc).first) {
                        router.submit_latency_arb_order(*signal, kelly->position_size_usdc / signal->polymarket_price);
                        std::string la_dir = (signal->token_id == signal->market.yes_token_id) ? "YES" : "NO";
                        store.push_signal(fmt::format("LA SIGNAL {} {} | Edge:{:.3f} Fair:{:.3f} PM:{:.4f}",
                            signal->asset, la_dir, signal->edge, signal->fair_value, signal->polymarket_price));
                        store.push_telemetry(fmt::format("[LA] FILLED {} {} @ {:.4f} | ${:.2f}",
                            signal->asset, la_dir, signal->polymarket_price, kelly->position_size_usdc));
                    }
                }
            }
        };

        auto poly_feed = std::make_shared<PolymarketFeed>(feed_ioc, feed_ctx, store);

        btc_feed->set_tick_callback([&](const std::string& sym, double) { eval_la_for_asset(sym); });
        eth_feed->set_tick_callback([&](const std::string& sym, double) { eval_la_for_asset(sym); });
        sol_feed->set_tick_callback([&](const std::string& sym, double) { eval_la_for_asset(sym); });

        poly_feed->set_tick_callback([&](const std::string& token_id) {
            std::lock_guard<std::mutex> lock(detector_mutex);
            double now_ms = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
            if (dh_detector) {
                auto signal = dh_detector->evaluate(now_ms);
                if (signal) {
                    double max_allowed_usdc = risk_manager.get_current_balance() * 0.08;
                    double size_shares = max_allowed_usdc / signal->combined_price;
                    if (risk_manager.can_open_dh_position(signal->combined_price * size_shares).first) {
                        router.submit_dump_hedge_order(*signal, size_shares);
                        store.push_signal(fmt::format("DH SIGNAL {} | YES:{:.4f} NO:{:.4f} SUM:{:.4f} DISC:{:.1f}%",
                            signal->asset, signal->yes_price, signal->no_price,
                            signal->combined_price, signal->discount * 100.0));
                        store.push_telemetry(fmt::format("[DH] FILLED {} @ {:.4f} | {:.2f} shares | ${:.2f}",
                            signal->asset, signal->yes_price, size_shares, signal->yes_price * size_shares));
                        store.push_telemetry(fmt::format("[DH] FILLED {} @ {:.4f} | {:.2f} shares | ${:.2f}",
                            signal->asset, signal->no_price, size_shares, signal->no_price * size_shares));
                    }
                }
            }
        });

        // Start feeds only after all callbacks are ready
        btc_feed->start();
        eth_feed->start();
        sol_feed->start();
        poly_feed->start();

        std::atomic<bool> is_refreshing{false};
        auto refresh_markets = [&]() {
            if (is_refreshing.exchange(true)) return;
            try {
                gamma_ioc.restart();
                std::vector<MarketInfo> all_m;
                auto b = gamma.fetch_updown_markets("btc");
                auto e = gamma.fetch_updown_markets("eth");
                auto s = gamma.fetch_updown_markets("sol");
                all_m.insert(all_m.end(), b.begin(), b.end());
                all_m.insert(all_m.end(), e.begin(), e.end());
                all_m.insert(all_m.end(), s.begin(), s.end());
                store.update_markets(all_m);
                {
                    std::lock_guard<std::mutex> lock(detector_mutex);
                    for (auto& det : la_detectors) { det->set_active_markets(all_m); det->set_entry_price_range(0.20, 0.80); }
                    dh_detector = std::make_unique<DumpHedgeDetector>(store, all_m, 0.93, 0.02, 60.0, 30.0);
                }
                std::vector<std::string> tokens;
                for (const auto& m : all_m) { tokens.push_back(m.yes_token_id); tokens.push_back(m.no_token_id); }
                if (!tokens.empty()) poly_feed->subscribe(tokens);
                store.push_telemetry(fmt::format("MARKETS REFRESHED | {} markets | {} tokens",
                    all_m.size(), tokens.size()));
            } catch (const std::exception& e) {
                spdlog::error("Refresh markets failed: {}", e.what());
            }
            is_refreshing = false;
        };

        refresh_markets();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        auto last_market_refresh = std::chrono::system_clock::now();

        while (true) {
            auto loop_start = std::chrono::system_clock::now();
            if (loop_start - last_market_refresh > std::chrono::seconds(60)) {
                last_market_refresh = loop_start;
                std::thread([&refresh_markets]() { refresh_markets(); }).detach();
            }
            risk_manager.is_trading_allowed(); // Trigger resume checks even if no signals fire
            check_and_close_positions(risk_manager, store, router, exit_cfg);
            std::cout << store.get_dashboard_json() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        feed_work.reset();
        if (feed_thread.joinable()) feed_thread.join();
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}
