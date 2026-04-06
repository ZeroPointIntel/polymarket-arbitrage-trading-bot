"""
OPENCLAW · POLYMARKET ARB BOT

Usage:
    python main.py [--paper | --live]

Strategies:
    latency_arb  — exploit ~2.7s Binance→Polymarket lag
    dump_hedge   — buy YES+NO when combined < $1.00 (structural arb)
    both         — run both simultaneously

Environment:
    Copy .env.example to .env and fill in your credentials before running.
    Always run in paper mode for at least 20 trades before going live.
"""

import argparse
import asyncio
import signal
import sys
import time
import uuid
from typing import Optional

# Force UTF-8 on Windows terminals (Git Bash / CMD use CP1252 by default,
# which cannot encode box-drawing / arrow characters used in log messages).
try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass  # Non-reconfigurable stream (e.g. piped output) — ignore

from config import BotConfig
from core.binance_ws import BinanceWebSocketFeed
from core.dump_hedge_detector import DumpHedgeDetector, DumpHedgeSignal
from core.edge_detector import EdgeDetector, TradeSignal
from core.polymarket_client import PolymarketClient, MarketInfo
from core.polymarket_ws import PolymarketWSFeed
from integration.openclaw import OpenClawIntegration
from integration.telegram import TelegramAlerter
from risk.kelly import KellySizer
from risk.risk_manager import DumpHedgePosition, Position, RiskManager, TradingStatus
from utils.dashboard import render_dashboard
from utils.logger import get_logger, print_banner, setup_logging

logger = get_logger(__name__)


class PolymarketArbitrageBot:
    """
    Main orchestrator for the OpenClaw Polymarket Arb Bot.

    Supports three strategies (set via STRATEGY in .env):
      - latency_arb  : exploit ~2.7s Binance→Polymarket lag
      - dump_hedge   : buy YES+NO when combined ask < $1.00
      - both         : run both simultaneously

    Coordinates all subsystems:
      1. BinanceWebSocketFeed  — real-time asset prices (latency_arb only)
      2. EdgeDetector          — sigmoid model + 4-layer filter chain
      3. DumpHedgeDetector     — combined price scanner
      4. KellySizer            — position sizing (Kelly or fixed bet)
      5. RiskManager           — drawdown limits, daily halt, kill switch
      6. PolymarketClient      — CLOB order execution + market discovery
      7. OpenClawIntegration   — bidirectional AI agent communication
      8. TelegramAlerter       — real-time push notifications
    """

    LOOP_INTERVAL_SECONDS = 0.05  # Main loop frequency: 20 Hz (faster signal detection)
    POSITION_CHECK_INTERVAL = 0.5  # Check exit conditions every 0.5s (was 1.0s)
    COMMAND_POLL_INTERVAL = 10.0   # How often to poll OpenClaw for commands
    HEARTBEAT_INTERVAL = 2.0       # Dashboard refresh every 2s (prices update live)

    def __init__(self, config: BotConfig) -> None:
        self.config = config
        self._running = False
        self._start_time = 0.0
        self._shutdown_requested = False   # set by SIGINT; triggers confirmation prompt
        self._last_position_check = 0.0
        self._last_command_poll = 0.0
        self._last_heartbeat = 0.0
        self._last_kill_log = 0.0
        self._loop_count = 0

        # Per-asset position lock: asset → order_id of the current open position.
        # A new signal for an asset is blocked until its position is fully closed.
        self._asset_open_position: dict = {}  # e.g. {"btc": "0xabc...", "sol": None}
        # Separate lock for dump-hedge positions (asset → dh_id).
        self._asset_open_dh_position: dict = {}

        # Track consecutive sell failures per order_id.
        # After MAX_SELL_RETRIES failures a Telegram alert fires and we stop retrying.
        self._sell_fail_count: dict = {}
        self.MAX_SELL_RETRIES = 3

        # Initialize all subsystems
        logger.info("Initializing bot subsystems... (strategy=%s)", config.strategy)

        # ── Binance feeds ─────────────────────────────────────────────────────
        # Only needed for latency-arb (requires real-time price to detect lag).
        # dump_hedge strategy works purely from Polymarket pricing — no Binance needed.
        _ASSET_SYMBOLS = {"btc": "BTCUSDT", "eth": "ETHUSDT", "sol": "SOLUSDT", "xrp": "XRPUSDT"}
        _use_binance = config.strategy in ("latency_arb", "both")
        if _use_binance:
            self._feeds: dict = {
                asset: BinanceWebSocketFeed(
                    symbol=_ASSET_SYMBOLS[asset],
                    reconnect_delay=config.binance_reconnect_delay,
                )
                for asset in config.trading_markets
                if asset in _ASSET_SYMBOLS
            }
            # Primary feed used for startup readiness check (prefer BTC, else first available)
            self.binance_feed = self._feeds.get("btc") or next(iter(self._feeds.values()))
        else:
            self._feeds = {}
            self.binance_feed = None
        # Convenience aliases used in a few places
        self.btc_feed = self._feeds.get("btc")
        self.eth_feed = self._feeds.get("eth")
        self.sol_feed = self._feeds.get("sol")

        logger.info(
            "Active markets: %s | Binance feeds: %s",
            ", ".join(config.trading_markets).upper(),
            "enabled" if _use_binance else "disabled (dump_hedge only)",
        )

        self.polymarket_client = PolymarketClient(
            host=config.polymarket_host,
            chain_id=config.polymarket_chain_id,
            private_key=config.polymarket_private_key,
            funder=config.polymarket_funder,
            signature_type=config.polymarket_signature_type,
            paper_mode=config.paper_mode,
            trade_window_minutes=config.trade_window_minutes,
        )

        # ── Edge detector (latency-arb) ───────────────────────────────────────
        if config.strategy in ("latency_arb", "both"):
            self.edge_detector: Optional[EdgeDetector] = EdgeDetector(
                feeds=self._feeds,
                polymarket_client=self.polymarket_client,
                min_edge_threshold=config.edge_min_edge_threshold,
                lag_window_seconds=config.edge_lag_window_seconds,
                cooldown_seconds=config.edge_cooldown_seconds,
                min_market_liquidity=config.edge_min_market_liquidity,
                trade_window_minutes=config.trade_window_minutes,
                min_entry_price=config.edge_min_entry_price,
                max_entry_price=config.edge_max_entry_price,
                min_fair_value_strength=config.edge_min_fair_value_strength,
                min_seconds_remaining=config.edge_min_seconds_remaining,
            )
        else:
            self.edge_detector = None

        # ── Dump-hedge detector ───────────────────────────────────────────────
        if config.strategy in ("dump_hedge", "both"):
            self.dh_detector: Optional[DumpHedgeDetector] = DumpHedgeDetector(
                polymarket_client=self.polymarket_client,
                assets=config.trading_markets,
                sum_target=config.dh_sum_target,
                min_discount=config.dh_min_discount,
                min_market_liquidity=config.edge_min_market_liquidity,
                trade_window_minutes=config.trade_window_minutes,
                cooldown_seconds=config.dh_cooldown_seconds,
            )
        else:
            self.dh_detector = None

        # KELLY_ENABLED=true  → Kelly formula (fixed_bet_usdc=0 disables fixed mode)
        # KELLY_ENABLED=false → fixed bet (use RISK_FIXED_BET_USDC)
        _fixed_bet = 0.0 if config.kelly_enabled else config.risk_fixed_bet_usdc
        self.kelly_sizer = KellySizer(
            kelly_fraction=config.risk_kelly_fraction,
            max_position_fraction=config.risk_max_position_fraction,
            fixed_bet_usdc=_fixed_bet,
        )

        # Live balance fetched async in run() — use config default here
        starting_balance = config.paper_starting_balance
        self.risk_manager = RiskManager(
            starting_balance=starting_balance,
            max_position_fraction=config.risk_max_position_fraction,
            daily_loss_limit=config.risk_daily_loss_limit,
            total_drawdown_kill=config.risk_total_drawdown_kill,
            max_concurrent_positions=config.risk_max_concurrent_positions,
        )

        # Polymarket CLOB WebSocket feed (real-time price_change events)
        # Replaces REST polling for price lookups: ~10-50ms vs ~200-500ms
        self.pm_ws_feed = PolymarketWSFeed(
            on_price_change=self._on_pm_price_change,
        )
        # Attach WS feed to client so get_market_price() uses cache first
        self.polymarket_client.attach_ws_feed(self.pm_ws_feed)

        self.openclaw = OpenClawIntegration(
            api_key=config.openclaw_api_key,
            api_url=config.openclaw_api_url,
            agent_id=config.openclaw_agent_id,
            report_interval_seconds=config.openclaw_report_interval,
            enabled=config.openclaw_enabled,
        )

        self.telegram = TelegramAlerter(
            bot_token=config.telegram_bot_token,
            chat_id=config.telegram_chat_id,
            enabled=config.telegram_enabled,
        )

        # Register OpenClaw command handlers
        self._register_openclaw_commands()

        logger.info(
            "Bot initialized | Mode: %s | Balance: $%.2f",
            "PAPER" if config.paper_mode else "LIVE",
            starting_balance,
        )

    # ─────────────────────────────────────────────────────────────────────────
    # Lifecycle
    # ─────────────────────────────────────────────────────────────────────────

    async def run(self) -> None:
        """Start the bot. Runs indefinitely until stopped."""
        self._running = True
        self._start_time = time.time()

        # Start Binance feeds in background (only when strategy needs them)
        feed_tasks = [asyncio.create_task(feed.start()) for feed in self._feeds.values()]

        # Start Polymarket CLOB WebSocket feed in background
        pm_ws_task = asyncio.create_task(self.pm_ws_feed.start())
        logger.info("Polymarket CLOB WebSocket feed starting...")

        # Wait for first Binance price tick — only required for latency-arb.
        # dump_hedge strategy works entirely from Polymarket prices and can start immediately.
        if self.binance_feed is not None:
            logger.info("Waiting for Binance price feed (REST bootstrap + WebSocket)...")
            for _ in range(350):  # Up to 35 seconds
                if self.binance_feed.latest_price is not None:
                    break
                await asyncio.sleep(0.1)

            if self.binance_feed.latest_price is None:
                logger.error(
                    "Failed to receive BTC price from Binance within 35 seconds. "
                    "Check your internet connection and firewall settings. "
                    "Ensure ports 9443 and 443 are not blocked."
                )
                self._running = False
                for t in feed_tasks:
                    t.cancel()
                return

            primary_asset = self.config.trading_markets[0].upper()
            logger.info(
                "Binance %s feed connected. %s: $%.2f",
                primary_asset, primary_asset, self.binance_feed.latest_price,
            )
        else:
            logger.info("Binance feeds skipped (strategy=%s)", self.config.strategy)
        # Sync live balance from blockchain before starting (live mode only)
        if not self.config.paper_mode:
            live_balance = await self.polymarket_client.get_portfolio_balance()
            if live_balance is not None and live_balance > 0:
                # Set baseline BEFORE update_balance so _check_risk_thresholds
                # sees 0% daily loss instead of comparing against stale config value.
                self.risk_manager._daily_starting_balance = live_balance
                self.risk_manager.update_balance(live_balance)
                # Clear daily halt if triggered by stale config balance at startup
                if self.risk_manager._status == TradingStatus.DAILY_HALT:
                    self.risk_manager._status = TradingStatus.ACTIVE
                    self.risk_manager._kill_reason = None
                    logger.info("Daily halt cleared — baseline reset to live $%.2f", live_balance)
                logger.info("Live balance synced from blockchain: $%.2f USDC", live_balance)
            else:
                logger.warning(
                    "Could not fetch live balance — using config default $%.2f. "
                    "Verify POLYMARKET_FUNDER address is correct.",
                    self.risk_manager.current_balance,
                )

        # Send startup notifications
        self.telegram.send_startup(
            paper_mode=self.config.paper_mode,
            balance=self.risk_manager.current_balance,
        )
        cfg = self.config
        self.openclaw.push_startup_notification({
            "paper_mode": cfg.paper_mode,
            "strategy": cfg.strategy,
            "balance": self.risk_manager.current_balance,
            "markets": cfg.trading_markets,
            "trade_window_minutes": cfg.trade_window_minutes,
            # Latency-arb params (only relevant when strategy includes latency_arb)
            "min_edge_threshold": cfg.edge_min_edge_threshold,
            "lag_window_seconds": cfg.edge_lag_window_seconds,
            "kelly_fraction": cfg.risk_kelly_fraction,
            # Dump-hedge params (only relevant when strategy includes dump_hedge)
            "dh_sum_target": cfg.dh_sum_target,
            "dh_fixed_bet_usdc": cfg.dh_fixed_bet_usdc,
            "dh_early_exit_fraction": cfg.dh_early_exit_profit_fraction,
        })

        # Main trading loop
        logger.info("Entering main trading loop...")
        try:
            await self._main_loop()
        except asyncio.CancelledError:
            logger.info("Main loop cancelled.")
        finally:
            for feed in self._feeds.values():
                feed.stop()
            await self.pm_ws_feed.stop()
            for t in feed_tasks:
                t.cancel()
            pm_ws_task.cancel()
            logger.info("Bot shutdown complete.")

    async def _main_loop(self) -> None:
        """Core trading loop: evaluate edges, execute trades, manage positions."""
        while self._running:
            loop_start = time.time()

            # Ctrl+C confirmation — checked before any other logic so the
            # prompt appears on the very next loop iteration after SIGINT.
            if self._shutdown_requested:
                self._shutdown_requested = False
                confirmed = await self._confirm_shutdown()
                if confirmed:
                    self._running = False
                    break
                # User chose to continue — resume normally

            try:
                self._loop_count += 1
                now = time.time()

                # 1. Poll OpenClaw for commands
                if now - self._last_command_poll >= self.COMMAND_POLL_INTERVAL:
                    await self.openclaw.poll_and_execute_commands()
                    self._last_command_poll = now

                # 2. Check risk state
                if not self.risk_manager.is_trading_allowed:
                    status = self.risk_manager.status
                    if status == TradingStatus.KILLED:
                        # Log once, then only every 60s — avoids CRITICAL spam every second
                        if now - self._last_kill_log >= 60.0:
                            logger.critical(
                                "Kill switch active — bot is halted. "
                                "Balance: $%.2f | Use reset_kill_switch(confirm=True) to resume.",
                                self.risk_manager.current_balance,
                            )
                            self._last_kill_log = now

                    # Dashboard must refresh here too — the normal heartbeat below is
                    # never reached because of the `continue`, so status stays stale.
                    if now - self._last_heartbeat >= self.HEARTBEAT_INTERVAL:
                        render_dashboard(
                            feeds=self._feeds,
                            risk_state=self.risk_manager.get_state(),
                            open_positions=dict(self.risk_manager._open_positions),
                            asset_locks=dict(self._asset_open_position),
                            edge_stats=self.edge_detector.get_stats() if self.edge_detector else {},
                            active_markets={
                                **(dict(self.edge_detector._active_market_by_asset) if self.edge_detector else {}),
                                **(dict(self.dh_detector._active_market_by_asset) if self.dh_detector else {}),
                            },
                            edge_detector=self.edge_detector,
                            paper_mode=self.config.paper_mode,
                            log_lines=self.config.dashboard_log_lines,
                            trade_window_minutes=self.config.trade_window_minutes,
                            strategy=self.config.strategy,
                            open_dh_positions=dict(self.risk_manager._open_dh_positions),
                            dh_detector=self.dh_detector,
                            engine_config={
                                "dh_sum_target":            self.config.dh_sum_target,
                                "dh_min_discount":          self.config.dh_min_discount,
                                "dh_fixed_bet_usdc":        self.config.dh_fixed_bet_usdc,
                                "max_concurrent_positions": self.config.risk_max_concurrent_positions,
                                "daily_loss_limit":         self.config.risk_daily_loss_limit,
                            },
                            telegram_enabled=self.config.telegram_enabled,
                            uptime_s=now - self._start_time,
                        )
                        self._last_heartbeat = now

                    await asyncio.sleep(1.0)
                    continue

                # 3. Evaluate for edge signal (latency-arb)
                if self.edge_detector is not None:
                    signal = await self.edge_detector.evaluate()
                    if signal:
                        await self._handle_signal(signal)

                # 3b. Evaluate for dump-hedge signal
                if self.dh_detector is not None:
                    dh_signal = await self.dh_detector.evaluate()
                    if dh_signal:
                        await self._handle_dh_signal(dh_signal)

                # 4. Periodically check open positions for exit conditions
                if now - self._last_position_check >= self.POSITION_CHECK_INTERVAL:
                    await self._check_open_positions()
                    if self.dh_detector is not None:
                        await self._check_open_dh_positions()
                    self._last_position_check = now

                # 5. Push performance summary to OpenClaw
                risk_state = self.risk_manager.get_state()
                edge_stats = self.edge_detector.get_stats() if self.edge_detector else {}
                dh_stats = self.dh_detector.get_stats() if self.dh_detector else {}
                self.openclaw.push_performance_summary(
                    risk_state=risk_state,
                    edge_stats={**edge_stats, **dh_stats},
                    feed_stats=self.binance_feed.get_stats() if self.binance_feed else {},
                )

                # 6. Dashboard refresh
                if now - self._last_heartbeat >= self.HEARTBEAT_INTERVAL:
                    edge_stats = self.edge_detector.get_stats() if self.edge_detector else {}

                    # Refresh active markets and subscribe PM WS for all active assets
                    subs = getattr(self, "_pm_ws_subscribed_conditions", set())
                    if self.edge_detector is not None:
                        for asset in self.config.trading_markets:
                            mkt = await self.edge_detector._get_active_5m_market(asset)
                            if mkt and self.pm_ws_feed.is_connected and mkt.condition_id not in subs:
                                self._subscribe_pm_ws_to_market(mkt)
                                subs.add(mkt.condition_id)
                    self._pm_ws_subscribed_conditions = subs

                    render_dashboard(
                        feeds=self._feeds,
                        risk_state=self.risk_manager.get_state(),
                        open_positions=dict(self.risk_manager._open_positions),
                        asset_locks=dict(self._asset_open_position),
                        edge_stats=edge_stats,
                        active_markets={
                            **(dict(self.edge_detector._active_market_by_asset) if self.edge_detector else {}),
                            **(dict(self.dh_detector._active_market_by_asset) if self.dh_detector else {}),
                        },
                        edge_detector=self.edge_detector,
                        paper_mode=self.config.paper_mode,
                        log_lines=self.config.dashboard_log_lines,
                        trade_window_minutes=self.config.trade_window_minutes,
                        strategy=self.config.strategy,
                        open_dh_positions=dict(self.risk_manager._open_dh_positions),
                        dh_detector=self.dh_detector,
                        engine_config={
                            "dh_sum_target":            self.config.dh_sum_target,
                            "dh_min_discount":          self.config.dh_min_discount,
                            "dh_fixed_bet_usdc":        self.config.dh_fixed_bet_usdc,
                            "max_concurrent_positions": self.config.risk_max_concurrent_positions,
                            "daily_loss_limit":         self.config.risk_daily_loss_limit,
                        },
                        telegram_enabled=self.config.telegram_enabled,
                        uptime_s=now - self._start_time,
                    )
                    self._last_heartbeat = now

            except Exception as exc:
                logger.error("Unexpected error in main loop: %s", exc, exc_info=True)
                self.telegram.send_risk_alert(
                    "UnexpectedError",
                    str(exc),
                    severity="ERROR",
                )

            # Maintain loop frequency
            elapsed = time.time() - loop_start
            sleep_time = max(0.0, self.LOOP_INTERVAL_SECONDS - elapsed)
            await asyncio.sleep(sleep_time)

    def stop(self) -> None:
        """Gracefully stop the bot."""
        logger.info("Shutdown requested.")
        self._running = False
        for feed in self._feeds.values():
            feed.stop()

    async def _confirm_shutdown(self) -> bool:
        """
        Pause the dashboard, warn the user about open positions, and ask
        whether to exit or continue.

        Returns True  → caller should stop the bot.
        Returns False → user chose to continue; caller resumes the loop.
        """
        from utils.dashboard import _stop_live

        la_count = len(self.risk_manager._open_positions)
        dh_count = len(self.risk_manager._open_dh_positions)
        total    = la_count + dh_count

        # Pause the Rich Live TUI so we can write to stdout cleanly.
        _stop_live()

        LINE = "─" * 57

        if total > 0:
            print(f"\n┌{LINE}┐")
            print(f"│  ⚠  CTRL+C — OPEN POSITIONS DETECTED                  │")
            print(f"├{LINE}┤")
            print(f"│  Latency-Arb  :  {la_count:<3}  position(s)                    │")
            print(f"│  Dump-Hedge   :  {dh_count:<3}  position(s)                    │")
            print(f"│  Total open   :  {total:<3}                                │")
            print(f"├{LINE}┤")
            print(f"│  [E] + ENTER  →  exit  (positions left unresolved)    │")
            print(f"│  [C] + ENTER  →  continue running                     │")
            print(f"└{LINE}┘")
        else:
            print(f"\n┌{LINE}┐")
            print(f"│  CTRL+C — no open positions · shutting down…          │")
            print(f"└{LINE}┘")
            return True

        try:
            raw = await asyncio.get_event_loop().run_in_executor(
                None, lambda: input("Your choice [E/C]: ").strip().lower()
            )
            return raw.startswith("e")
        except (EOFError, KeyboardInterrupt):
            # Second Ctrl+C while prompt is showing → force exit
            print("\nForce-exit.")
            return True

    # ─────────────────────────────────────────────────────────────────────────
    # Signal Handling & Trade Execution
    # ─────────────────────────────────────────────────────────────────────────

    async def _handle_signal(self, signal: TradeSignal) -> None:
        """Process a trade signal: size, validate, and execute."""
        # Block if this asset already has an open (unresolved) position.
        existing = self._asset_open_position.get(signal.asset)
        if existing:
            logger.debug(
                "Asset %s locked by open position %s — new signal ignored",
                signal.asset.upper(), existing[:12],
            )
            return

        logger.info("Processing signal: %s", signal)

        # NORMAL MODE: buy in the direction of the signal.
        # Signal says UP → buy YES, DOWN → buy NO.
        # Use signal.current_polymarket_price (fresh price fetched by edge detector)
        # NOT market.yes_price/no_price which may be stale/default 0.50.
        if signal.direction == "UP":
            trade_token_id    = signal.market.yes_token_id
        else:
            trade_token_id    = signal.market.no_token_id
        trade_entry_price = signal.current_polymarket_price

        # Calculate Kelly position size
        win_prob = signal.fair_value_estimate
        logger.info(
            "Kelly inputs: balance=$%.2f win_prob=%.3f entry_price=%.4f token=%s",
            self.risk_manager.current_balance, win_prob, trade_entry_price,
            trade_token_id[:16] if trade_token_id else "None",
        )
        kelly = self.kelly_sizer.calculate(
            bankroll=self.risk_manager.current_balance,
            win_probability=win_prob,
            current_price=trade_entry_price,
        )

        if kelly is None:
            logger.warning("Kelly returned None — check inputs above.")
            return

        # Risk check
        allowed, reason = self.risk_manager.can_open_position(kelly.position_size_usdc)
        if not allowed:
            logger.warning("Position blocked by risk manager: %s", reason)
            return

        # Execute the order
        order_result = await self.polymarket_client.place_market_order(
            token_id=trade_token_id,
            side="BUY",
            amount_usdc=kelly.position_size_usdc,
            market_info=signal.market,
        )

        if not order_result.success:
            logger.error("Order failed: %s", order_result.error)
            self.telegram.send_risk_alert(
                "OrderFailed",
                f"Order error: {order_result.error}",
                severity="ERROR",
            )
            # Set per-asset cooldown so we don't immediately retry the same asset
            self.edge_detector._last_signal_time[signal.asset] = time.time()
            return

        # Acquire per-asset lock so no second order is placed until this closes.
        self._asset_open_position[signal.asset] = order_result.order_id

        # Register position with risk manager
        position = Position(
            order_id=order_result.order_id,
            token_id=trade_token_id,
            market_question=signal.market.question,
            side="BUY",
            entry_price=order_result.price,
            size_shares=order_result.size,
            cost_usdc=order_result.size * order_result.price,
            opened_at=order_result.timestamp,
            asset=signal.asset,
            direction=signal.direction,  # "UP" or "DOWN"
            paper_mode=self.config.paper_mode,
        )
        self.risk_manager.register_trade_open(position)

        # Notifications
        self.telegram.send_trade_opened(
            order_id=order_result.order_id,
            market_question=signal.market.question,
            side=signal.side,
            price=order_result.price,
            size_usdc=kelly.position_size_usdc,
            edge=signal.edge,
            btc_move=signal.btc_move,
            paper_mode=self.config.paper_mode,
            asset=signal.asset,
            direction=signal.direction,
        )
        self.openclaw.push_trade_opened(
            order_id=order_result.order_id,
            market_question=signal.market.question,
            side=signal.side,
            price=order_result.price,
            size_usdc=kelly.position_size_usdc,
            edge=signal.edge,
            paper_mode=self.config.paper_mode,
        )

        logger.info(
            "Trade executed: %s | $%.2f USDC | Edge: %.4f | Kelly: %.4f",
            order_result.order_id,
            kelly.position_size_usdc,
            signal.edge,
            kelly.fractional_kelly,
        )

    async def _check_open_positions(self) -> None:
        open_orders = self.polymarket_client.get_open_orders()
        open_order_ids = {o.get("id") for o in open_orders}

        for order_id, position in list(self.risk_manager._open_positions.items()):

            # Skip if the entry order is still pending in the order book
            # (not yet filled — no position to sell yet)
            if order_id in open_order_ids:
                logger.debug(
                    "Position %s still pending in order book — skipping exit check.",
                    order_id[:20],
                )
                continue

            current_price = await self.polymarket_client.get_market_price(
                position.token_id, "SELL"
            )
            if current_price is None:
                continue

            should_exit = False
            exit_reason = ""

            entry_price = position.entry_price
            current_pnl_pct = (current_price - entry_price) / entry_price

            cfg = self.config
            # Priority order: near-resolution exits first (market about to settle),
            # then take profit / stop loss, then timeout.
            if current_price >= cfg.near_win_price:
                should_exit = True
                exit_reason = f"Near resolution YES: price={current_price:.3f} >= {cfg.near_win_price}"
            elif current_price <= cfg.near_loss_price:
                should_exit = True
                exit_reason = f"Near resolution NO: price={current_price:.3f} <= {cfg.near_loss_price}"
            elif current_price >= cfg.take_profit_price or current_pnl_pct >= cfg.take_profit_pnl:
                should_exit = True
                exit_reason = f"Take profit: price={current_price:.3f} pnl={current_pnl_pct:+.1%}"
            elif cfg.stop_loss_pnl < 0 and current_pnl_pct <= cfg.stop_loss_pnl:
                should_exit = True
                exit_reason = f"Stop loss: pnl={current_pnl_pct:+.1%} limit={cfg.stop_loss_pnl:+.1%}"
            elif time.time() - position.opened_at > cfg.position_timeout_seconds:
                should_exit = True
                exit_reason = f"Position timeout ({cfg.position_timeout_seconds:.0f}s)"

            if should_exit:
                # ── Sell-retry gate ──────────────────────────────────────────
                fail_count = self._sell_fail_count.get(order_id, 0)
                if fail_count >= self.MAX_SELL_RETRIES:
                    # Already notified after max retries — do not retry.
                    logger.debug(
                        "Skipping sell for %s — max retries (%d) already reached.",
                        order_id[:20], self.MAX_SELL_RETRIES,
                    )
                    continue

                logger.info("Closing position %s: %s", order_id[:20], exit_reason)

                # Step 1: Submit SELL order to Polymarket
                sell_result = await self.polymarket_client.place_market_order(
                    token_id=position.token_id,
                    side="SELL",
                    amount_usdc=position.size_shares,
                )

                # Step 2: Handle sell failure with retry counting
                if not sell_result.success:
                    new_count = fail_count + 1
                    self._sell_fail_count[order_id] = new_count
                    logger.error(
                        "SELL FAILED for %s | Attempt %d/%d | Error: %s",
                        order_id[:20], new_count, self.MAX_SELL_RETRIES, sell_result.error,
                    )
                    if new_count >= self.MAX_SELL_RETRIES:
                        # Exhausted all retries — alert operator
                        self.telegram.send_risk_alert(
                            "MaxSellRetriesReached",
                            f"Failed to sell position {order_id[:12]} after "
                            f"{self.MAX_SELL_RETRIES} attempts. "
                            f"Please check manually on Polymarket!\n"
                            f"Last error: {sell_result.error}",
                            severity="CRITICAL",
                        )
                        logger.critical(
                            "MAX SELL RETRIES (%d) REACHED for %s — "
                            "manual intervention required!",
                            self.MAX_SELL_RETRIES, order_id[:20],
                        )
                    continue  # do not update state — retry on next iteration

                # Step 3: Update internal state with actual fill price
                # actual_proceeds = real cash received from sell fill
                # (on-chain shares × fill price) — not the estimated entry shares.
                actual_exit_price = sell_result.price
                actual_proceeds = sell_result.size * sell_result.price if not self.config.paper_mode else None
                closed = self.risk_manager.register_trade_close(
                    order_id=order_id,
                    exit_price=actual_exit_price,
                    actual_proceeds_usdc=actual_proceeds,
                )

                if closed is None:
                    # register_trade_close rejected (e.g. invalid price) — keep position tracked
                    logger.error(
                        "register_trade_close rejected for %s — position still tracked.",
                        order_id[:20],
                    )
                    continue

                # Sell succeeded — clear retry counter and release asset lock.
                # Use position.asset directly (reliable) instead of scanning by order_id.
                self._sell_fail_count.pop(order_id, None)
                if position.asset:
                    self._asset_open_position.pop(position.asset, None)
                    logger.info("Asset lock released: %s", position.asset.upper())
                    # Reset per-asset cooldown from close time so the bot waits
                    # a full cooldown_seconds before re-entering this asset.
                    if self.edge_detector is not None:
                        self.edge_detector.reset_cooldown(position.asset)

                # Always notify on close — pnl_usdc may be None in paper mode
                # but the trade still happened and should be reported.
                self.telegram.send_trade_closed(
                    order_id=order_id,
                    pnl_usdc=closed.pnl_usdc,
                    exit_price=actual_exit_price,
                    duration_seconds=closed.duration_seconds,
                    paper_mode=self.config.paper_mode,
                    entry_price=position.entry_price,
                    size_usdc=position.cost_usdc,
                    exit_reason=exit_reason,
                    asset=position.asset,
                    direction=position.direction,
                )
                if closed.pnl_usdc is not None:
                    self.openclaw.push_trade_closed(
                        order_id=order_id,
                        pnl_usdc=closed.pnl_usdc,
                        exit_price=actual_exit_price,
                        duration_seconds=closed.duration_seconds,
                        paper_mode=self.config.paper_mode,
                    )

                if self.risk_manager.status == TradingStatus.KILLED:
                    reason = self.risk_manager._kill_reason or "Unknown"
                    self.telegram.send_kill_switch_alert(
                        reason,
                        risk_state=self.risk_manager.get_state(),
                    )
                    self.openclaw.push_kill_switch_alert(reason)

    # ─────────────────────────────────────────────────────────────────────────
    # Dump-Hedge Execution
    # ─────────────────────────────────────────────────────────────────────────

    async def _handle_dh_signal(self, signal: DumpHedgeSignal) -> None:
        """Open a two-leg dump-hedge position: buy both YES and NO simultaneously."""
        asset = signal.asset

        # Block if this asset already has an open DH position.
        if self._asset_open_dh_position.get(asset):
            logger.debug(
                "Asset %s already has open DH position %s — signal ignored",
                asset.upper(), self._asset_open_dh_position[asset],
            )
            return

        cfg = self.config
        # Polymarket enforces a $1.00 minimum per order leg.
        # size_shares must be large enough that BOTH legs each cost ≥ $1.00,
        # while total cost stays within DH_FIXED_BET_USDC.
        POLYMARKET_MIN_LEG_USDC = 1.00
        # Minimum shares to satisfy the $1.00 floor on each leg
        yes_min = POLYMARKET_MIN_LEG_USDC / signal.yes_price
        no_min  = POLYMARKET_MIN_LEG_USDC / signal.no_price
        size_from_min = max(yes_min, no_min)  # satisfies both legs' minimums
        # Maximum shares allowed by the configured bet budget
        size_from_budget = cfg.dh_fixed_bet_usdc / signal.combined_price
        size_shares = min(size_from_min, size_from_budget)

        combined_cost = signal.combined_price * size_shares
        locked_profit = signal.discount * size_shares

        # Guard: if budget can't cover the $1 minimum per leg, skip this signal
        yes_cost = size_shares * signal.yes_price
        no_cost  = size_shares * signal.no_price
        if yes_cost < POLYMARKET_MIN_LEG_USDC or no_cost < POLYMARKET_MIN_LEG_USDC:
            logger.warning(
                "DH signal skipped — budget $%.2f too small for $1.00 min per leg "
                "(YES $%.2f + NO $%.2f required). Raise DH_FIXED_BET_USDC.",
                cfg.dh_fixed_bet_usdc,
                1.00 / signal.yes_price * signal.combined_price,
                1.00 / signal.no_price  * signal.combined_price,
            )
            return

        # Risk check
        allowed, reason = self.risk_manager.can_open_dh_position(combined_cost)
        if not allowed:
            logger.warning("DH position blocked by risk manager: %s", reason)
            return

        dh_id = f"dh_{asset}_{uuid.uuid4().hex[:8]}"

        logger.info(
            "Opening DH position %s | %s | YES@%.3f + NO@%.3f | "
            "%.4f shares | Cost: $%.2f | Locked profit: $%.2f",
            dh_id, asset.upper(), signal.yes_price, signal.no_price,
            size_shares, combined_cost, locked_profit,
        )

        # Place YES (BUY) leg
        yes_result = await self.polymarket_client.place_market_order(
            token_id=signal.yes_token_id,
            side="BUY",
            amount_usdc=size_shares * signal.yes_price,
            market_info=signal.market,
        )
        if not yes_result.success:
            logger.error("DH YES leg failed: %s", yes_result.error)
            self.dh_detector.reset_cooldown(asset)
            return

        # Place NO (BUY) leg
        no_result = await self.polymarket_client.place_market_order(
            token_id=signal.no_token_id,
            side="BUY",
            amount_usdc=size_shares * signal.no_price,
            market_info=signal.market,
        )
        if not no_result.success:
            logger.error(
                "DH NO leg failed after YES filled: %s — position is unhedged! "
                "Manual intervention may be required.",
                no_result.error,
            )
            self.telegram.send_risk_alert(
                "DHNoLegFailed",
                f"DH YES leg filled but NO leg failed for {asset.upper()}. "
                f"YES order: {yes_result.order_id}. Error: {no_result.error}",
                severity="CRITICAL",
            )
            return

        # Acquire per-asset DH lock
        self._asset_open_dh_position[asset] = dh_id

        position = DumpHedgePosition(
            dh_id=dh_id,
            yes_order_id=yes_result.order_id,
            no_order_id=no_result.order_id,
            yes_token_id=signal.yes_token_id,
            no_token_id=signal.no_token_id,
            market_question=signal.market.question,
            asset=asset,
            yes_entry_price=yes_result.price,
            no_entry_price=no_result.price,
            combined_entry_price=yes_result.price + no_result.price,
            size_shares=size_shares,
            combined_cost_usdc=(yes_result.price + no_result.price) * size_shares,
            locked_profit_usdc=signal.discount * size_shares,
            opened_at=time.time(),
            paper_mode=cfg.paper_mode,
        )
        self.risk_manager.register_dh_open(position)

        self.telegram.send_message(
            f"{'📄' if cfg.paper_mode else '🔒'} *DH Opened* | {asset.upper()} | "
            f"YES@{yes_result.price:.3f} + NO@{no_result.price:.3f} | "
            f"Combined: {position.combined_entry_price:.3f} | "
            f"Locked: ${locked_profit:.2f} ({signal.discount_pct:.1%} ROI) | "
            f"{'PAPER' if cfg.paper_mode else 'LIVE'}"
        )
        self.openclaw.push_dh_opened(
            dh_id=dh_id,
            asset=asset,
            yes_price=yes_result.price,
            no_price=no_result.price,
            combined_price=position.combined_entry_price,
            locked_profit_usdc=position.locked_profit_usdc,
            size_shares=position.size_shares,
            combined_cost_usdc=position.combined_cost_usdc,
            paper_mode=cfg.paper_mode,
        )

    async def _check_open_dh_positions(self) -> None:
        """Check DH positions for early-exit or timeout, then close if triggered."""
        cfg = self.config

        for dh_id, position in list(self.risk_manager._open_dh_positions.items()):
            age = time.time() - position.opened_at

            # Get current SELL prices for both legs
            yes_sell = await self.polymarket_client.get_market_price(
                position.yes_token_id, "SELL"
            )
            no_sell = await self.polymarket_client.get_market_price(
                position.no_token_id, "SELL"
            )

            # If prices unavailable (404 = market resolved/expired), force-close
            # at locked profit. When a binary market resolves, one leg pays $1.00
            # and the other $0.00 → net PnL = locked_profit regardless of direction.
            if yes_sell is None or no_sell is None:
                if age >= 30.0:  # Give 30s grace period before assuming resolved
                    logger.info(
                        "DH %s: prices unavailable after %.0fs — market likely resolved, "
                        "closing at locked profit $%.2f",
                        dh_id, age, position.locked_profit_usdc,
                    )
                    yes_exit = position.yes_entry_price
                    no_exit  = position.no_entry_price
                    # Use locked_profit as the PnL (structural guarantee at resolution)
                    actual_proceeds = position.combined_cost_usdc + position.locked_profit_usdc
                    closed = self.risk_manager.register_dh_close(
                        dh_id=dh_id,
                        yes_exit_price=yes_exit,
                        no_exit_price=no_exit,
                        exit_reason="Market resolved (prices unavailable)",
                        actual_proceeds_usdc=actual_proceeds,
                    )
                    if closed:
                        self._asset_open_dh_position.pop(position.asset, None)
                        if self.dh_detector:
                            self.dh_detector.reset_cooldown(position.asset)
                        self.telegram.send_message(
                            f"{'📄' if cfg.paper_mode else '✅'} *DH Resolved* | {position.asset.upper()} | "
                            f"PnL: ${position.locked_profit_usdc:+.2f} | Market resolved | "
                            f"Duration: {closed.duration_seconds:.0f}s"
                        )
                        self.openclaw.push_dh_closed(
                            dh_id=dh_id,
                            asset=position.asset,
                            pnl_usdc=closed.pnl_usdc or position.locked_profit_usdc,
                            exit_reason="Market resolved (prices unavailable)",
                            duration_seconds=closed.duration_seconds,
                            paper_mode=cfg.paper_mode,
                        )
                continue

            combined_sell = yes_sell + no_sell
            # Refresh age after the two await price-fetch calls for accuracy
            age = time.time() - position.opened_at

            should_exit = False
            exit_reason = ""

            # Early-exit: realised fraction of locked profit is good enough
            profit_so_far = (combined_sell - position.combined_entry_price) * position.size_shares
            target_profit = position.locked_profit_usdc * cfg.dh_early_exit_profit_fraction
            if profit_so_far >= target_profit:
                should_exit = True
                exit_reason = (
                    f"Early exit: profit ${profit_so_far:.2f} >= "
                    f"{cfg.dh_early_exit_profit_fraction:.0%} of locked ${position.locked_profit_usdc:.2f}"
                )

            # Timeout
            elif age >= cfg.dh_timeout_seconds:
                should_exit = True
                exit_reason = f"DH timeout ({cfg.dh_timeout_seconds:.0f}s)"

            if not should_exit:
                continue

            logger.info("Closing DH %s: %s", dh_id, exit_reason)

            # Sell YES leg
            yes_sell_result = await self.polymarket_client.place_market_order(
                token_id=position.yes_token_id,
                side="SELL",
                amount_usdc=position.size_shares,
            )
            # Sell NO leg
            no_sell_result = await self.polymarket_client.place_market_order(
                token_id=position.no_token_id,
                side="SELL",
                amount_usdc=position.size_shares,
            )

            actual_proceeds: Optional[float] = None
            if not cfg.paper_mode:
                yes_proceeds = yes_sell_result.size * yes_sell_result.price if yes_sell_result.success else 0.0
                no_proceeds  = no_sell_result.size * no_sell_result.price  if no_sell_result.success  else 0.0
                actual_proceeds = yes_proceeds + no_proceeds

            yes_exit = yes_sell_result.price if yes_sell_result.success else yes_sell
            no_exit  = no_sell_result.price  if no_sell_result.success  else no_sell

            closed = self.risk_manager.register_dh_close(
                dh_id=dh_id,
                yes_exit_price=yes_exit,
                no_exit_price=no_exit,
                exit_reason=exit_reason,
                actual_proceeds_usdc=actual_proceeds,
            )

            if closed is None:
                continue

            # Release asset lock
            self._asset_open_dh_position.pop(position.asset, None)
            if self.dh_detector:
                self.dh_detector.reset_cooldown(position.asset)

            pnl = closed.pnl_usdc or 0.0
            self.telegram.send_message(
                f"{'📄' if cfg.paper_mode else '✅'} *DH Closed* | {position.asset.upper()} | "
                f"PnL: ${pnl:+.2f} | {exit_reason[:60]} | "
                f"Duration: {closed.duration_seconds:.0f}s | "
                f"{'PAPER' if cfg.paper_mode else 'LIVE'}"
            )
            self.openclaw.push_dh_closed(
                dh_id=dh_id,
                asset=position.asset,
                pnl_usdc=pnl,
                exit_reason=exit_reason,
                duration_seconds=closed.duration_seconds,
                paper_mode=cfg.paper_mode,
            )

            if self.risk_manager.status == TradingStatus.KILLED:
                reason = self.risk_manager._kill_reason or "Unknown"
                self.telegram.send_kill_switch_alert(
                    reason,
                    risk_state=self.risk_manager.get_state(),
                )

    # ─────────────────────────────────────────────────────────────────────────
    # OpenClaw Command Handlers
    # ─────────────────────────────────────────────────────────────────────────

    def _register_openclaw_commands(self) -> None:
        """Register handlers for commands that can be sent from the OpenClaw agent."""

        def handle_pause(cmd):
            reason = cmd.parameters.get("reason", "Paused by OpenClaw agent")
            self.risk_manager.pause(reason)
            self.telegram.send_risk_alert("TradingPaused", reason, severity="WARNING")

        def handle_resume(cmd):
            success = self.risk_manager.resume()
            if success:
                self.telegram.send_message("▶️ *Trading Resumed* by OpenClaw agent.")

        def handle_status(cmd):
            state = self.risk_manager.get_state()
            self.telegram.send_performance_summary(
                balance=state.current_balance,
                total_pnl=state.total_pnl,
                total_pnl_pct=state.total_pnl_pct * 100,
                daily_pnl=state.daily_pnl,
                win_rate=state.win_rate * 100,
                total_trades=state.total_trades,
                drawdown_pct=state.drawdown_from_peak_pct * 100,
                paper_mode=self.config.paper_mode,
            )

        def handle_reset_kill(cmd):
            confirm = cmd.parameters.get("confirm", False)
            self.risk_manager.reset_kill_switch(confirm=confirm)

        def handle_stop(cmd):
            logger.warning("STOP command received from OpenClaw agent.")
            self.stop()

        self.openclaw.register_command_handler("pause", handle_pause)
        self.openclaw.register_command_handler("resume", handle_resume)
        self.openclaw.register_command_handler("status", handle_status)
        self.openclaw.register_command_handler("reset_kill_switch", handle_reset_kill)
        self.openclaw.register_command_handler("stop", handle_stop)

    # ─────────────────────────────────────────────────────────────────────────
    # Polymarket WebSocket Callbacks
    # ─────────────────────────────────────────────────────────────────────────

    def _on_pm_price_change(self, token_id: str, price: float, side: str, ts: float) -> None:
        """
        Callback invoked by PolymarketWSFeed on every price_change event.
        Updates the cached yes/no price for whichever active market owns this token.
        Checks both edge_detector (latency_arb) and dh_detector (dump_hedge) caches.
        """
        detectors = []
        if self.edge_detector is not None:
            detectors.append(self.edge_detector._active_market_by_asset)
        if self.dh_detector is not None:
            detectors.append(self.dh_detector._active_market_by_asset)

        for cache in detectors:
            for mkt in cache.values():
                if mkt is None:
                    continue
                if token_id == mkt.yes_token_id:
                    mkt.yes_price = price
                    return
                if token_id == mkt.no_token_id:
                    mkt.no_price = price
                    return

    def _subscribe_pm_ws_to_market(self, market: MarketInfo) -> None:
        """
        Subscribe the Polymarket WS feed to a new active market.
        subscribe() is synchronous — call it directly, no create_task.
        """
        self.pm_ws_feed.subscribe(market.condition_id)
        logger.info(
            "PM WS subscribed to market: %s (conditionId: %s)",
            market.question[:50],
            market.condition_id[:16],
        )


# ─────────────────────────────────────────────────────────────────────────────
# CLI Entrypoint
# ─────────────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Polymarket Latency Arbitrage Bot — OpenClaw Edition",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python main.py                    # Run in paper mode (default)
  python main.py --live             # Run in live mode (requires credentials)
  python main.py --paper            # Explicitly run in paper mode
  python main.py --log-level DEBUG  # Enable verbose logging
        """,
    )
    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument(
        "--paper",
        action="store_true",
        default=False,
        help="Run in paper trading mode (default). No real orders placed.",
    )
    mode_group.add_argument(
        "--live",
        action="store_true",
        default=False,
        help="Run in live trading mode. Requires POLYMARKET_PRIVATE_KEY and POLYMARKET_FUNDER.",
    )
    parser.add_argument(
        "--log-level",
        default=None,
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Override log level from config.",
    )
    parser.add_argument(
        "--balance",
        type=float,
        default=None,
        help="Override starting paper balance (USDC).",
    )
    return parser.parse_args()


async def main() -> None:
    args = parse_args()
    config = BotConfig()

    # Apply CLI overrides
    if args.live:
        config.paper_mode = False
    elif args.paper:
        config.paper_mode = True
    # Default: respect PAPER_MODE env variable

    if args.log_level:
        config.log_level = args.log_level

    if args.balance:
        config.paper_starting_balance = args.balance

    print_banner()

    # Validate configuration
    try:
        config.validate()
    except ValueError as exc:
        logger.critical("Configuration error: %s", exc)
        sys.exit(1)

    # Add file handler to root logger (console handlers still active until dashboard starts)
    setup_logging(config.log_file, config.log_level)

    mode_str = "PAPER MODE" if config.paper_mode else "LIVE MODE"
    logger.info("Starting Polymarket Arbitrage Bot in %s", mode_str)

    if not config.paper_mode:
        logger.warning(
            "⚠️  LIVE MODE ACTIVE — Real funds will be used. "
            "Ensure you have completed paper trading validation first."
        )

    # Initialize and run bot
    bot = PolymarketArbitrageBot(config)

    # Handle graceful shutdown on SIGINT/SIGTERM.
    # SIGINT (Ctrl+C) → set flag so the main loop shows the confirmation prompt.
    # SIGTERM (system shutdown, Docker stop) → stop immediately, no prompt.
    def sigint_handler(_sig, _frame):
        if bot._shutdown_requested:
            # Second Ctrl+C while prompt is already pending → force stop.
            logger.warning("Second SIGINT received — forcing shutdown.")
            bot.stop()
        else:
            bot._shutdown_requested = True

    def sigterm_handler(_sig, _frame):
        logger.info("SIGTERM received — shutting down.")
        bot.stop()

    signal.signal(signal.SIGINT,  sigint_handler)
    signal.signal(signal.SIGTERM, sigterm_handler)

    await bot.run()


if __name__ == "__main__":
    asyncio.run(main())
