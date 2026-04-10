"""
╔══════════════════════════════════════════════════════════════════════════════╗
║  TELEGRAM ALERTING                                                           ║
║  Real-time notifications for trade executions, risk alerts, and bot status. ║
╚══════════════════════════════════════════════════════════════════════════════╝
"""

import time
from concurrent.futures import ThreadPoolExecutor
from typing import Any, Optional

import requests

from utils.logger import get_logger

logger = get_logger(__name__)

# Telegram Bot API base URL
TELEGRAM_API_BASE = "https://api.telegram.org/bot{token}"


class TelegramAlerter:
    """
    Sends formatted alert messages to a Telegram chat via the Bot API.

    All send operations are non-blocking and wrapped in try/except —
    a Telegram API failure will never halt the trading loop.

    Rate limiting: Telegram allows ~30 messages/second per bot. The alerter
    enforces a minimum 1-second gap between messages to stay well within limits.
    """

    def __init__(
        self,
        bot_token: str,
        chat_id: str,
        enabled: bool = True,
        min_interval_seconds: float = 1.0,
    ) -> None:
        self.bot_token = bot_token
        self.chat_id = chat_id
        self.enabled = enabled and bool(bot_token) and bool(chat_id)
        self.min_interval_seconds = min_interval_seconds

        self._last_send_time: float = 0.0
        self._messages_sent: int = 0
        self._base_url = TELEGRAM_API_BASE.format(token=bot_token)
        # Single-worker pool — messages are queued and sent in order without
        # blocking the asyncio event loop.
        self._executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix="telegram")

        if self.enabled:
            logger.info("TelegramAlerter initialized | Chat ID: %s", chat_id)
        else:
            logger.info("TelegramAlerter DISABLED (no token/chat ID configured).")

    # ─────────────────────────────────────────────────────────────────────────
    # Alert Methods
    # ─────────────────────────────────────────────────────────────────────────

    def send_startup(
        self,
        paper_mode: bool,
        balance: float,
        strategy: str = "latency_arb",
        markets: Optional[list] = None,
    ) -> None:
        """Send a startup notification."""
        _STRATEGY_LABELS = {
            "latency_arb": "Latency Arbitrage",
            "dump_hedge":  "Dump-Hedge",
            "both":        "Latency Arb + Dump-Hedge",
        }
        strat_label = _STRATEGY_LABELS.get(strategy, strategy.upper())
        mode_label  = "📄 PAPER MODE" if paper_mode else "🔴 LIVE MODE"
        markets_str = ", ".join(m.upper() for m in markets) if markets else "N/A"
        self._send(
            f"🤖 *Polymarket Arb Bot Started*\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Mode: {mode_label}\n"
            f"Balance: `${balance:,.2f} USDC`\n"
            f"Strategy: {strat_label}\n"
            f"Markets: {markets_str}\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"_OpenClaw Edition — Bot is running._"
        )

    def send_trade_opened(
        self,
        order_id: str,
        market_question: str,
        side: str,
        price: float,
        size_usdc: float,
        edge: float,
        btc_move: float,
        paper_mode: bool,
        asset: str = "",
        direction: str = "",
    ) -> None:
        """Send a trade opened notification."""
        mode_icon = "📄" if paper_mode else "✅"
        move_icon = "📈" if btc_move > 0 else "📉"
        dir_icon  = "▲" if direction == "UP" else ("▼" if direction == "DOWN" else "")
        token_str = f"  {asset.upper()} {dir_icon} {direction}" if asset and direction else ""

        self._send(
            f"{mode_icon} *Trade Opened*{token_str}\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Market: `{market_question[:60]}`\n"
            f"Price: `{price:.4f}` ({price * 100:.1f}¢)\n"
            f"Size: `${size_usdc:.2f} USDC`\n"
            f"Edge: `{edge:.4f}` ({edge * 100:.2f}%)\n"
            f"Move: {move_icon} `${btc_move:+.2f}`\n"
            f"Order ID: `{order_id[:20]}`",
            priority=True,
        )

    def send_trade_closed(
        self,
        order_id: str,
        pnl_usdc: Optional[float],
        exit_price: float,
        duration_seconds: float,
        paper_mode: bool,
        entry_price: float = 0.0,
        size_usdc: float = 0.0,
        exit_reason: str = "",
        asset: str = "",
        direction: str = "",
    ) -> None:
        """Send a trade closed notification (priority — always delivered)."""
        mode_icon = "📄" if paper_mode else "🏁"
        if pnl_usdc is None:
            result_icon = "⚪"
            pnl_line = "PnL: `n/a`"
        else:
            result_icon = "🟢" if pnl_usdc >= 0 else "🔴"
            pnl_pct = (pnl_usdc / size_usdc * 100) if size_usdc > 0 else 0.0
            pnl_line = f"PnL: `${pnl_usdc:+.2f} USDC` ({pnl_pct:+.1f}%)"

        mins, secs = divmod(int(duration_seconds), 60)
        dur_str = f"{mins}m {secs}s" if mins > 0 else f"{secs}s"

        # Header line: "📄 Trade Closed 🟢  BTC ▲ UP"
        dir_icon = "▲" if direction == "UP" else ("▼" if direction == "DOWN" else "")
        token_str = f"  {asset.upper()} {dir_icon} {direction}" if asset and direction else ""

        lines = [
            f"{mode_icon} *Trade Closed* {result_icon}{token_str}",
            "━━━━━━━━━━━━━━━━━━━━",
            pnl_line,
        ]
        if entry_price > 0:
            lines.append(f"Entry → Exit: `{entry_price:.4f}` → `{exit_price:.4f}`")
        else:
            lines.append(f"Exit Price: `{exit_price:.4f}`")
        lines.append(f"Duration: `{dur_str}`")
        if exit_reason:
            lines.append(f"Reason: _{exit_reason}_")
        lines.append(f"Order ID: `{order_id[:20]}`")

        self._send("\n".join(lines), priority=True)

    def send_performance_summary(
        self,
        balance: float,
        total_pnl: float,
        total_pnl_pct: float,
        daily_pnl: float,
        win_rate: float,
        total_trades: int,
        drawdown_pct: float,
        paper_mode: bool,
    ) -> None:
        """Send a periodic performance summary."""
        mode_label = "PAPER" if paper_mode else "LIVE"
        trend_icon = "📈" if total_pnl >= 0 else "📉"
        self._send(
            f"📊 *Performance Summary* [{mode_label}]\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Balance: `${balance:,.2f} USDC`\n"
            f"Total PnL: {trend_icon} `${total_pnl:+,.2f}` ({total_pnl_pct:+.1f}%)\n"
            f"Daily PnL: `${daily_pnl:+.2f}`\n"
            f"Win Rate: `{win_rate:.1f}%` ({total_trades} trades)\n"
            f"Max Drawdown: `{drawdown_pct:.1f}%`"
        )

    def send_risk_alert(self, alert_type: str, message: str, severity: str = "WARNING") -> None:
        """Send a risk management alert."""
        severity_icons = {
            "INFO": "ℹ️",
            "WARNING": "⚠️",
            "ERROR": "🚨",
            "CRITICAL": "🔥",
        }
        icon = severity_icons.get(severity, "⚠️")
        self._send(
            f"{icon} *Risk Alert: {alert_type}*\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Severity: *{severity}*\n"
            f"Message: {message}"
        )

    def send_kill_switch_alert(self, reason: str, risk_state: Any = None) -> None:
        """Send a critical kill switch notification with session summary."""
        lines = [
            "🔥🔥🔥 *KILL SWITCH TRIGGERED* 🔥🔥🔥",
            "━━━━━━━━━━━━━━━━━━━━",
            "ALL TRADING HALTED",
            "",
            f"Reason:\n`{reason}`",
        ]
        if risk_state is not None:
            pnl_icon = "📈" if risk_state.total_pnl >= 0 else "📉"
            wr = risk_state.win_rate * 100 if hasattr(risk_state, "win_rate") else 0.0
            lines += [
                "",
                "📊 *Session Summary*",
                f"Balance: `${risk_state.current_balance:,.2f}` (Peak: `${risk_state.peak_balance:,.2f}`)",
                f"Total PnL: {pnl_icon} `${risk_state.total_pnl:+.2f}` ({risk_state.total_pnl_pct:+.1%})",
                f"Daily PnL: `${risk_state.daily_pnl:+.2f}`",
                f"Drawdown: `{risk_state.drawdown_from_peak_pct:.1%}`",
                f"Trades: `{risk_state.total_trades}` | Win Rate: `{wr:.0f}%`",
            ]
        lines += [
            "",
            "⚠️ *Manual investigation required before restarting.*",
            "Call `reset_kill_switch(confirm=True)` to resume.",
        ]
        self._send("\n".join(lines), priority=True)

    def send_daily_halt_alert(self, daily_loss_pct: float, balance: float) -> None:
        """Send a daily halt notification."""
        self._send(
            f"🛑 *Daily Trading Halt*\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Daily loss limit reached: `{daily_loss_pct:.1f}%`\n"
            f"Current balance: `${balance:,.2f} USDC`\n"
            f"Trading will resume at midnight UTC."
        )

    def send_message(self, text: str) -> None:
        """Send a raw text message (for custom notifications)."""
        self._send(text)

    # ─────────────────────────────────────────────────────────────────────────
    # Internal
    # ─────────────────────────────────────────────────────────────────────────

    def _send(self, text: str, priority: bool = False) -> None:
        """Queue a message for delivery via the background thread pool.

        Priority messages (trade opened/closed, kill switch) bypass the rate
        limit and are always queued.  Non-priority messages are dropped when
        sent within min_interval_seconds of the previous message.

        The actual HTTP call happens in a single-worker ThreadPoolExecutor so
        the asyncio event loop is never blocked — even a 10-second Telegram
        timeout won't stall the trading loop.
        """
        if not self.enabled:
            logger.debug("Telegram disabled — message not sent: %.80s", text)
            return

        if not priority:
            elapsed = time.time() - self._last_send_time
            if elapsed < self.min_interval_seconds:
                logger.debug(
                    "Telegram rate limit: dropping non-priority message (%.1fs since last)",
                    elapsed,
                )
                return

        # Update send time optimistically before queuing so rapid non-priority
        # messages don't all queue up.
        self._last_send_time = time.time()
        self._executor.submit(self._http_send, text)

    def _http_send(self, text: str) -> None:
        """Synchronous HTTP POST executed in the thread pool."""
        try:
            response = requests.post(
                f"{self._base_url}/sendMessage",
                json={
                    "chat_id": self.chat_id,
                    "text": text,
                    "parse_mode": "Markdown",
                    "disable_web_page_preview": True,
                },
                timeout=10,
            )
            response.raise_for_status()
            self._messages_sent += 1
            logger.debug("Telegram message sent (#%d)", self._messages_sent)
        except requests.exceptions.RequestException as exc:
            logger.warning("Telegram send failed (non-critical): %s", exc)
