# POLYMARKET ARBITRAGE BOT — OPENCLAW EDITION

> Two independent arbitrage strategies for Polymarket binary prediction markets on Polygon.

[![Python](https://img.shields.io/badge/Python-3.9+-blue)](https://python.org)
[![Polygon](https://img.shields.io/badge/Network-Polygon_Mainnet-purple)](https://polygon.technology)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED?logo=docker)](https://docs.docker.com)

---

## Table of Contents

- [What This Bot Does](#what-this-bot-does)
- [Strategies](#strategies)
  - [Latency Arb](#latency-arb)
  - [Dump Hedge](#dump-hedge)
  - [Fair Value Model](#fair-value-model)
- [Signal Validation Filters](#signal-validation-filters)
- [Supported Assets & Windows](#supported-assets--windows)
- [Configuration Reference](#configuration-reference)
  - [Strategy](#strategy)
  - [Trading Mode](#trading-mode)
  - [Markets](#markets)
  - [Dump Hedge Parameters](#dump-hedge-parameters)
  - [Edge Detection](#edge-detection)
  - [Risk Management](#risk-management)
  - [Stop Loss / Take Profit](#stop-loss--take-profit)
  - [Kelly Criterion](#kelly-criterion)
  - [Telegram Notifications](#telegram-notifications)
  - [OpenClaw Integration](#openclaw-integration)
- [Risk Protection Layers](#risk-protection-layers)
- [Dashboard](#dashboard)
- [Project Structure](#project-structure)
- [Disclaimer](#disclaimer)

---

## What This Bot Does

The bot watches Polymarket binary prediction markets (e.g. "Will BTC be higher in 5 minutes?") and places trades when it detects a statistical edge. It runs two strategies independently or simultaneously:

- **Latency Arb** — exploits the ~2.7-second lag between Binance price moves and Polymarket's oracle update
- **Dump Hedge** — buys both YES and NO simultaneously when their combined price falls below $1.00, locking in a guaranteed structural profit

Both strategies execute through the Polymarket CLOB API using Fill-And-Kill (FAK) orders — no resting limit orders. Positions are tracked, sized by Kelly Criterion or a fixed USDC amount, and protected by multi-layer risk controls.

---

## Strategies

### Latency Arb

Polymarket relies on a Chainlink oracle to determine resolution prices. This oracle updates approximately every 2.7 seconds — during which Polymarket token prices have not yet reflected a Binance price move. The bot detects this lag and enters the trade before the oracle catches up.

```
  Binance WebSocket              Bot (<50ms)             Polymarket CLOB
  ─────────────────    ──────────────────────────    ────────────────────
  BTC: $83,000         Detects +$500 in 2.7s         "Bitcoin Up or Down"
  BTC: $83,500   ───►  Sigmoid model: P(UP)=0.65 ──► UP token still at 0.50
                       Edge = 0.65 − 0.50 = 0.15     (oracle not yet updated)
                       Buy UP @ 0.50 before update    ↓
                                                 Market corrects to 0.68
                                                 Exit at Take Profit ✓
```

This strategy requires a live Binance WebSocket feed and benefits significantly from low network latency. A VPS near a Binance server (e.g. Singapore) reduces end-to-end lag from ~200ms to under 15ms, which is the difference between catching the window and missing it.

---

### Dump Hedge

Binary prediction markets resolve to exactly $1.00 (winner) or $0.00 (loser). Because of this, the combined cost of YES + NO must eventually equal exactly $1.00 at resolution. When market inefficiencies push the combined ask below $1.00, the difference is a locked structural profit — regardless of which direction the asset moves.

```
  Polymarket CLOB                Bot                      At Resolution
  ─────────────────    ──────────────────────────    ────────────────────
  YES ask = 0.420      combined = 0.970               YES resolves $1.00
  NO  ask = 0.550  ──► discount = $0.030/share   ──►  NO  resolves $0.00
                       Buy BOTH legs                   Collect $1.00/share
                       Cost = $0.970/share             Profit = $0.030/share
                                                       = 3.09% locked ✓
```

This strategy does **not** require a Binance feed. It works entirely from Polymarket CLOB prices and fires whenever the combined YES + NO ask falls below `DH_SUM_TARGET`.

The bot can exit early — before market resolution — if the combined sell price rises enough to realise a target fraction of the locked profit. This is controlled by `DH_EARLY_EXIT_PROFIT_FRACTION`.

---

### Fair Value Model

For latency arb, the bot estimates the true probability of a YES outcome using a **time-aware sigmoid model**:

```
P(UP) = sigmoid( (price_now − price_to_beat) / scale(t) )

scale(t) = base_scale × sqrt(t / window) + min_scale
```

- `price_to_beat` is the Binance asset price at window open (the reference price the Chainlink oracle will use for resolution)
- `price_now` is the current Binance price
- `scale(t)` shrinks as the window closes — the model becomes more confident as less time remains
- The sigmoid converts the normalised distance into a probability between 0 and 1

The **edge** is `fair_value − polymarket_price`. If this exceeds `EDGE_MIN_EDGE_THRESHOLD`, the bot trades in the direction the model implies.

---

## Signal Validation Filters

Every potential latency arb signal passes through five sequential filters. All must pass before a trade fires.

```
Binance price move detected
         │
         ▼
┌─────────────────────────────────────────────┐
│ 1. MIN PRICE MOVE                           │
│    abs(price_now − price_2.7s_ago)          │
│    > min_price_move (per asset)             │
└───────────────┬─────────────────────────────┘
                │ pass
                ▼
┌─────────────────────────────────────────────┐
│ 2. ENTRY ZONE                               │
│    0.38 ≤ current_token_price ≤ 0.62        │
│                                             │
│    Tokens outside this range already        │
│    reflect 10+ minutes of accumulated       │
│    market direction. The 2.7s lag cannot    │
│    overcome that evidence.                  │
└───────────────┬─────────────────────────────┘
                │ pass
                ▼
┌─────────────────────────────────────────────┐
│ 3. FAIR VALUE STRENGTH                      │
│    abs(fair_value − 0.50) ≥ 0.05            │
│                                             │
│    Model must output ≥55% conviction.       │
│    When price_now ≈ price_to_beat, the      │
│    sigmoid outputs ≈ 0.50 — no real signal. │
│    This filter blocks fake edges from cheap │
│    tokens (e.g. 0.15¢ creates apparent 35%  │
│    edge with zero model conviction).        │
└───────────────┬─────────────────────────────┘
                │ pass
                ▼
┌─────────────────────────────────────────────┐
│ 4. MINIMUM EDGE                             │
│    fair_value − token_price ≥ 0.05          │
│                                             │
│    Probability advantage must clear the     │
│    threshold even after the conviction      │
│    check above.                             │
└───────────────┬─────────────────────────────┘
                │ pass
                ▼
┌─────────────────────────────────────────────┐
│ 5. TIMING WINDOW                            │
│    seconds_remaining ≥ min_seconds          │
│    (auto: 20% of window = 60s / 180s)       │
│                                             │
│    Avoids entries in the final 20% of the   │
│    window — high variance, model less       │
│    reliable near resolution.                │
└───────────────┬─────────────────────────────┘
                │ pass
                ▼
           TRADE FIRES
```

---

## Supported Assets & Windows

| Asset | Binance Feed | Min move required in 2.7s |
|-------|-------------|--------------------------|
| BTC | `BTCUSDT@trade` | $5.00 |
| ETH | `ETHUSDT@trade` | $0.53 |
| SOL | `SOLUSDT@trade` | $0.05 |
| XRP | `XRPUSDT@trade` | $0.01 |

Each asset has an independent Binance WebSocket feed, market cache, and per-asset cooldown timer.

| `TRADE_WINDOW_MINUTES` | Market Series | Notes |
|------------------------|---------------|-------|
| `5` | BTC/ETH/SOL/XRP Up or Down — 5 Minutes | Highest volume, tightest spreads |
| `15` | BTC/ETH/SOL/XRP Up or Down — 15 Minutes | Less noise, signals fire less often |

---

## Configuration Reference

All settings are loaded from `.env`. See `.env.example` for the full template.

---

### Strategy

```env
STRATEGY=dump_hedge
```

| Value | What it runs | What it needs |
|-------|-------------|---------------|
| `latency_arb` | Binance lag exploitation only | Binance WebSocket + low-latency connection |
| `dump_hedge` | Structural YES+NO arb only | Polymarket REST only — no Binance needed |
| `both` | Both strategies simultaneously | Both of the above |

`dump_hedge` is the simpler starting point — no Binance dependency, no latency requirement, guaranteed locked profit on every entry.

---

### Trading Mode

```env
PAPER_MODE=true
PAPER_STARTING_BALANCE=1000.0
PAPER_SLIPPAGE_PCT=0.005
```

| Setting | What it means |
|---------|---------------|
| `PAPER_MODE=true` | Simulation only — orders are simulated locally, no real funds move |
| `PAPER_MODE=false` | Live trading — real USDC is spent on Polygon |
| `PAPER_STARTING_BALANCE` | Virtual balance used in paper mode for PnL and risk calculations |
| `PAPER_SLIPPAGE_PCT` | Random ±slippage applied to simulated paper fills (e.g. `0.005` = ±0.5%). Makes paper results more realistic by accounting for spread and execution imperfection |

Run paper mode for at least 200 trades and verify positive PnL before switching to live.

---

### Markets

```env
MARKETS=btc,eth,sol
```

Controls which assets the bot monitors. Each active market adds one Binance WebSocket connection (latency arb) and one Polymarket REST polling loop. More markets increase signal frequency but also API request volume.

---

### Dump Hedge Parameters

```env
DH_SUM_TARGET=0.93
DH_MIN_DISCOUNT=0.02
DH_FIXED_BET_USDC=20
DH_EARLY_EXIT_PROFIT_FRACTION=0.70
DH_COOLDOWN_SECONDS=30.0
```

| Variable | What it controls |
|----------|-----------------|
| `DH_SUM_TARGET` | Maximum combined YES+NO ask price to enter. `0.93` means the bot only trades when combined ≤ 93¢ (locking ≥ 7¢/share profit). Lower values = fewer but wider-margin trades. |
| `DH_MIN_DISCOUNT` | Minimum guaranteed discount per share. Guards against exchange fees consuming the entire margin. |
| `DH_FIXED_BET_USDC` | Total USDC per DH trade, split proportionally between YES and NO legs based on their ask prices. |
| `DH_EARLY_EXIT_PROFIT_FRACTION` | When the realised profit reaches this fraction of the locked profit, the bot closes both legs early rather than waiting for market resolution. `0.70` = exit once 70% of locked profit is in hand. |
| `DH_TIMEOUT_SECONDS` | Maximum age of an open DH position. Auto-derived as 90% of the trade window if not set. |
| `DH_COOLDOWN_SECONDS` | Minimum wait between DH signals on the same asset after a trade closes. |

**Position sizing:**
The bot calculates `shares = DH_FIXED_BET_USDC / combined_price`, then derives per-leg USDC amounts as `shares × yes_price` and `shares × no_price`. Both legs receive the same share count so that whichever side resolves to $1.00, the full locked profit is captured.

Polymarket enforces a $1.00 minimum per order. Signals where either leg would cost less than $1.00 are automatically skipped.

---

### Edge Detection

```env
EDGE_LAG_WINDOW_SECONDS=2.7
EDGE_MIN_EDGE_THRESHOLD=0.05
EDGE_COOLDOWN_SECONDS=15
EDGE_MIN_MARKET_LIQUIDITY=500
EDGE_MIN_ENTRY_PRICE=0.38
EDGE_MAX_ENTRY_PRICE=0.62
EDGE_MIN_FAIR_VALUE_STRENGTH=0.05
```

| Variable | What it controls |
|----------|-----------------|
| `EDGE_LAG_WINDOW_SECONDS` | How far back the bot looks in Binance price history to measure the move. Matches the documented oracle update frequency. Do not set below `2.0`. |
| `EDGE_MIN_EDGE_THRESHOLD` | Minimum probability advantage required to trade. Higher values = fewer but higher-conviction signals. `0.04` is aggressive, `0.08` is conservative. |
| `EDGE_COOLDOWN_SECONDS` | Per-asset cooldown after a trade opens. Prevents re-entering the same asset repeatedly on consecutive ticks of the same price move. |
| `EDGE_MIN_MARKET_LIQUIDITY` | Minimum USDC volume in the order book. Shallow markets have high slippage risk. |
| `EDGE_MIN_ENTRY_PRICE` / `EDGE_MAX_ENTRY_PRICE` | Entry zone filter. Tokens already far from 50¢ reflect accumulated directional conviction the 2.7s lag cannot overcome. Default `[0.38, 0.62]`. |
| `EDGE_MIN_FAIR_VALUE_STRENGTH` | Minimum model conviction (`abs(fair_value − 0.5)`). Blocks signals where the price is near `price_to_beat` and the sigmoid has no real directional view. |
| `EDGE_MIN_SECONDS_REMAINING` | Minimum remaining window time before entry. Auto-derived as 20% of the window (60s for 5-min, 180s for 15-min). Prevents late entries with degraded signal quality. |

---

### Risk Management

```env
RISK_MAX_POSITION_FRACTION=0.35
RISK_FIXED_BET_USDC=20
RISK_MAX_CONCURRENT_POSITIONS=3
RISK_DAILY_LOSS_LIMIT=0.20
RISK_TOTAL_DRAWDOWN_KILL=0.40
```

| Variable | What it controls |
|----------|-----------------|
| `RISK_MAX_POSITION_FRACTION` | Hard cap on any single trade as a fraction of current balance. A $1,000 balance with `0.35` cannot place a trade above $350. |
| `RISK_FIXED_BET_USDC` | Fixed USDC per latency arb trade when `KELLY_ENABLED=false`. |
| `RISK_MAX_CONCURRENT_POSITIONS` | Maximum combined open positions (LA + DH). New signals are blocked when this limit is reached. |
| `RISK_DAILY_LOSS_LIMIT` | If the daily balance loss exceeds this fraction, trading halts until midnight UTC. `0.20` = halt at −20% daily loss. |
| `RISK_TOTAL_DRAWDOWN_KILL` | Permanent trading halt if balance falls this far below the peak balance ever seen. `0.40` = kill at −40% from peak. Requires manual reset to resume. |

---

### Stop Loss / Take Profit

```env
TAKE_PROFIT_PRICE=0.72
TAKE_PROFIT_PNL=0.12
STOP_LOSS_PNL=-0.20
NEAR_WIN_PRICE=0.92
NEAR_LOSS_PRICE=0.08
```

Exit conditions are evaluated every 0.5 seconds in priority order:

| Priority | Condition | Meaning |
|----------|-----------|---------|
| 1 | `price ≥ NEAR_WIN_PRICE` | Token near full resolution — exit now, not worth the slippage risk of waiting |
| 2 | `price ≤ NEAR_LOSS_PRICE` | Token near zero — cut losses, recovery is unlikely |
| 3 | `price ≥ TAKE_PROFIT_PRICE` or `pnl% ≥ TAKE_PROFIT_PNL` | Take profit target reached |
| 4 | `pnl% ≤ STOP_LOSS_PNL` | Stop loss breached |
| 5 | `age ≥ POSITION_TIMEOUT_SECONDS` | Position too old — force close before oracle resolves |

`TAKE_PROFIT_PRICE` must be above the typical entry zone. With entries between 0.38–0.62, a take profit at 0.42 would never trigger for most entries. The default 0.72 gives the market room to move after entry.

---

### Kelly Criterion

```env
KELLY_ENABLED=false
RISK_KELLY_FRACTION=0.5
KELLY_ADAPTIVE_ENABLED=false
```

The Kelly Criterion is a formula for optimal bet sizing given an edge and odds:

```
f* = (p × b − q) / b       where b = (1 − price) / price
```

- `p` = estimated win probability (from the sigmoid model)
- `q` = 1 − p
- `b` = net odds (what you win if correct relative to what you risk)

The result `f*` is the theoretically optimal fraction of bankroll. Fractional Kelly (`× RISK_KELLY_FRACTION`, default 0.5) halves this to reduce variance.

When `KELLY_ENABLED=false`, the bot ignores Kelly and uses the fixed `RISK_FIXED_BET_USDC` amount instead. Fixed betting is simpler and recommended for small balances.

When `KELLY_ADAPTIVE_ENABLED=true`, the Kelly fraction automatically scales based on the bot's historical win rate:

| Observed win rate | Effective Kelly fraction |
|-------------------|--------------------------|
| < 45% | Floor value (cautious sizing) |
| 45–50% | Linear blend between floor and base |
| 50–55% | Base Kelly fraction |
| > 55% | Up to 1.25× base (confident sizing) |

This prevents over-sizing during losing streaks and allows larger positions when the strategy is performing well.

---

### Telegram Notifications

```env
TELEGRAM_BOT_TOKEN=YOUR_BOT_TOKEN
TELEGRAM_CHAT_ID=YOUR_NUMERIC_CHAT_ID
TELEGRAM_ENABLED=true
```

The bot sends real-time push notifications to a Telegram chat for every significant event:

| Event | What is reported |
|-------|-----------------|
| Bot started | Mode, strategy, balance, active markets |
| Trade opened (LA) | Asset, direction (UP/DOWN), entry price, edge %, move size, order ID |
| Trade closed (LA) | PnL (USDC + %), entry → exit price, duration, exit reason |
| DH opened | Asset, YES price, NO price, combined cost, locked profit |
| DH closed | Asset, PnL, exit reason, duration |
| Market resolved | Auto-close when CLOB returns 404 after 30s grace period |
| Kill switch | Reason, session summary (balance, PnL, win rate, drawdown) |
| Daily halt | Reason, current balance |
| Circuit breaker | Triggered, resume time |
| Daily summary | Midnight UTC: win rate with 95% confidence interval, total trades, PnL, uptime |

Close and kill-switch notifications bypass the rate limiter so they are never delayed or dropped.

---

### OpenClaw Integration

```env
OPENCLAW_ENABLED=false
OPENCLAW_API_KEY=YOUR_API_KEY
OPENCLAW_AGENT_ID=main
OPENCLAW_REPORT_INTERVAL=300
```

OpenClaw is an AI agent platform that enables bidirectional control of the bot. When enabled, the bot pushes trade events and performance data to OpenClaw, and polls for remote commands every 10 seconds.

Supported remote commands:

| Command | Effect |
|---------|--------|
| `pause` | Pause trading |
| `resume` | Resume from pause |
| `status` | Push current performance summary to Telegram |
| `reset_kill_switch` | Reset the kill switch (requires `confirm: true`) |
| `stop` | Graceful shutdown |

All OpenClaw API calls are fully skipped when `OPENCLAW_ENABLED=false`. There is no performance impact when disabled.

---

## Risk Protection Layers

The bot enforces four independent layers of capital protection:

```
Layer 1 — Position fraction cap
  No single trade exceeds RISK_MAX_POSITION_FRACTION × current_balance.
  Applied per-trade before the order is placed.

Layer 2 — Concurrent position limit
  At most RISK_MAX_CONCURRENT_POSITIONS open at once (LA + DH combined).
  Each asset can hold at most one LA position AND one DH position simultaneously.

Layer 3 — Daily loss limit (soft halt, auto-resets)
  If balance drops RISK_DAILY_LOSS_LIMIT below the day's starting balance,
  trading halts. Resets automatically at midnight UTC.

Layer 4 — Total drawdown kill switch (hard halt, manual reset)
  If balance drops RISK_TOTAL_DRAWDOWN_KILL below the peak balance ever seen,
  trading is permanently halted until the operator manually resets it.
```

Additionally, from v1.10:

**Circuit Breaker** — pauses trading automatically after a run of consecutive losses. Triggers when 3 or more of the last 5 closed trades were losers AND cumulative loss in that window exceeds 2%. Trading resumes automatically after a configurable pause period (default: 5 minutes).

---

## Dashboard

The bot runs a live terminal dashboard using the Rich alternate screen, similar to `htop`.

![Dashboard Preview](image/dashboard_preview.svg)

```
┌─── POLYMARKET ARB BOT  ·  DUMP HEDGE ─────────────────────────────────────────┐
│  2026-04-06 12:34:11 UTC  ◆ PAPER  ● ACTIVE  Uptime 00:12:34                  │
│  Balance $1000.00  Daily +$5.50  Total +$5.50  Open 1  Trades 3  (100% win)   │
└────────────────────────────────────────────────────────────────────────────────┘

┌─── ACTIVE MARKETS — 5 MIN ─────────────────────────────────┐ ┌─ OPEN POSITIONS ─┐
│  ASSET  YES BID  NO BID  SPREAD  COMBINED  DISCOUNT  REMAIN │ │ DH · BTC          │
│  BTC    0.4200   0.5500  0.0300  0.9700    3.09%     3:42   │ │ [DUMP-HEDGE]      │
│  ETH    0.4800   0.5050  0.0150  0.9850    1.52%     4:10   │ │ YES entry: 0.4200 │
│  SOL    —        —       —       —         no market  —     │ │ NO  entry: 0.5500 │
└────────────────────────────────────────────────────────────┘ │ Locked: $0.0150   │
                                                                 └───────────────────┘
┌─── ENGINE STATUS ───────┐ ┌─── RISK STATUS ──────────┐ ┌─── RECENT LOG ──────────┐
│  Strategy  DUMP HEDGE   │ │  Balance  $1000.00 USDC  │ │  12:34:02 INFO signal   │
│  Mode      PAPER        │ │  Daily    +$5.50          │ │  12:34:03 INFO opened   │
│  Window    5-min        │ │  Win Rate 3/3 (100%)      │ │  12:38:12 INFO skip     │
│  DH Det.   ● RUNNING    │ │  Open Pos 1 / 3           │ │  12:40:12 INFO closed   │
│  Sum Tgt   0.93         │ │  DH Trades 3 total        │ └─────────────────────────┘
│  Min Disc  0.02         │ │  Drawdown $0.00 (0.0%)    │
└─────────────────────────┘ └──────────────────────────┘
● RUNNING │ PAPER MODE │ POLYGON:137 │ STRATEGY: dump_hedge │ TELEGRAM: ✓ │ Ctrl+C
```

**Panels:**
- **Header** — UTC time, mode (PAPER/LIVE), trading status, uptime, balance, daily/total PnL, open count, win rate
- **Active Markets** — YES/NO bid prices, combined sum, discount %, time remaining for each active market
- **Open Positions** — one card per position. DH positions (purple border) show locked profit. LA positions (yellow border) show entry price and age
- **Engine Status** — current strategy, mode, window, detector running states, DH thresholds
- **Risk Status** — balance, daily/total PnL, win rate, open count, drawdown, daily loss limit usage. LA/DH PnL shown separately when both strategies have trades
- **Recent Log** — last N log lines (controlled by `DASHBOARD_LOG_LINES`)

**Ctrl+C behaviour:** if there are open positions when you press Ctrl+C, the dashboard pauses and asks for confirmation. Press `E` + Enter to exit (positions left unresolved), or `C` + Enter to keep running. A second Ctrl+C forces an immediate exit.

---

## Project Structure

```
polymarket-arbitrage-trading-bot/
│
├── main.py                  # Main orchestrator + trading loop (20 Hz)
├── config.py                # Environment variable loader & validator
├── healthcheck.py           # Pre-flight system checker
│
├── core/
│   ├── binance_ws.py           # Binance WebSocket feed — real-time prices + history buffer
│   ├── polymarket_client.py    # Polymarket CLOB API — orders, markets, prices
│   ├── edge_detector.py        # Latency arb signal engine — sigmoid model + filter chain
│   ├── dump_hedge_detector.py  # Dump hedge signal engine — combined price scanner
│   └── polymarket_ws.py        # Polymarket real-time order book WebSocket
│
├── risk/
│   ├── kelly.py             # Kelly Criterion + fixed bet position sizer
│   └── risk_manager.py      # Drawdown tracking, daily halt, kill switch, circuit breaker
│
├── integration/
│   ├── telegram.py          # Telegram push notifications
│   └── openclaw.py          # OpenClaw AI agent integration
│
├── utils/
│   ├── dashboard.py         # Rich Live terminal dashboard
│   ├── logger.py            # Rotating file + coloured console logger
│   └── retry.py             # Async/sync retry with full-jitter exponential backoff
│
└── trades/                  # Auto-generated: daily CSV trade exports (trades_YYYY-MM-DD.csv)
```

For a detailed technical breakdown of each component, data flows, and design decisions, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Disclaimer

This software is provided for educational and experimental purposes. Prediction market trading involves significant financial risk. Past performance does not guarantee future results. The arbitrage window narrows as more participants compete for the same inefficiencies. You are solely responsible for any financial losses. Always validate with paper trading before deploying real capital.
