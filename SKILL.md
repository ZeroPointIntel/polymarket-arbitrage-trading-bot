---
name: polymarket-arbitrage-trading-bot
description: Setup, configure, run, and modify the OPENCLAW Polymarket Arbitrage Bot. Use when the user asks to run the bot, change strategy parameters, manage credentials, add features, debug errors, or understand how the bot works.
---

# OPENCLAW · Polymarket Arbitrage Trading Bot

Python-based dual-strategy arbitrage bot for Polymarket binary prediction markets on Polygon. Source lives at `polymarket_bot/` in the project root.

## Project Structure

```
polymarket-arbitrage-trading-bot/
├── main.py                    ← Orchestrator — owns all components, runs the main loop (20 Hz asyncio)
├── config.py                  ← BotConfig dataclass + .env loader + validate()
├── healthcheck.py             ← Pre-flight check: Python version, packages, API connectivity
├── requirements.txt           ← Python dependencies
├── .env                       ← Active runtime config (never commit)
├── .env.examples              ← Reference config with all variables documented
│
├── core/
│   ├── binance_ws.py          ← Real-time price feed (WebSocket, auto-reconnect, parallel endpoint probe)
│   ├── edge_detector.py       ← Latency arb signal engine (sigmoid fair-value model, 5-filter chain)
│   ├── dump_hedge_detector.py ← Dump hedge signal engine (combined YES+NO price scanner)
│   ├── polymarket_client.py   ← CLOB API wrapper (market discovery, FAK orders, fills, redemption)
│   └── polymarket_ws.py       ← Optional Polymarket CLOB real-time WebSocket feed
│
├── risk/
│   ├── kelly.py               ← Position sizing (Kelly formula + fixed bet mode)
│   └── risk_manager.py        ← Kill switch, daily halt, drawdown tracking, balance state machine
│
├── integration/
│   ├── telegram.py            ← Push notifications (rate-limited, priority bypass for close/kill events)
│   └── openclaw.py            ← Bidirectional AI agent integration (push events, pull commands)
│
└── utils/
    ├── dashboard.py           ← Rich Live full-screen terminal dashboard (alternate screen buffer)
    ├── logger.py              ← Rotating file + coloured console handler
    └── retry.py               ← @async_retry / @sync_retry with full-jitter exponential backoff
```

## Strategies

### 1. Latency Arb (`STRATEGY=latency_arb`)
Exploits a documented ~2.7-second pricing lag between Binance and Polymarket.

**Signal filter chain (all 5 must pass):**
1. `abs(price_now - price_2.7s_ago) > min_price_move` (per asset)
2. Token price in entry zone: `0.38 ≤ price ≤ 0.62`
3. Sigmoid model conviction: `abs(fair_value - 0.50) ≥ EDGE_MIN_FAIR_VALUE_STRENGTH`
4. Edge: `fair_value - token_price ≥ EDGE_MIN_EDGE_THRESHOLD`
5. Time: `seconds_remaining ≥ EDGE_MIN_SECONDS_REMAINING`

**Fair value model:**
```
P(UP) = sigmoid( (price_now - price_to_beat) / scale(t) )
scale(t) = base_scale × sqrt(t / window_seconds) + min_scale
```

**Per-asset config (hardcoded in `edge_detector.py`):**
| Asset | base_scale | min_scale | min_price_move |
|-------|-----------|-----------|---------------|
| BTC   | 500.0     | 50.0      | $5.00         |
| ETH   | 30.0      | 3.0       | $0.53         |
| SOL   | 2.0       | 0.2       | $0.05         |
| XRP   | 0.3       | 0.05      | $0.01         |

Requires: Binance WebSocket feed, low-latency VPS (Singapore recommended).

### 2. Dump Hedge (`STRATEGY=dump_hedge`)
Buys both YES and NO when their combined ask price < $1.00. Exactly one token pays $1.00 at resolution, so any combined price under $1.00 locks in guaranteed profit.

```
YES ask = 0.42  ─┐
NO  ask = 0.55  ─┘  combined = 0.97 → discount = $0.03/share → profit = 3.09%
```

**No Binance feed required.** Works purely from Polymarket REST/WebSocket pricing.

### 3. Both (`STRATEGY=both`)
Runs both strategies simultaneously. Requires Binance feeds.

## Prerequisites

- Python 3.9+ (3.11 recommended)
- Polymarket account with USDC on **Polygon mainnet** (not Ethereum mainnet)
- Wallet private key (for live mode only)
- Minimum ~$5 USDC balance for live trading
- Low-latency internet (VPS near Singapore strongly recommended for latency arb)

> **Geo-restriction:** Polymarket is not available to US residents.

## Installation & Running

```bash
cd polymarket_bot

# Install dependencies (uses the extracted venv or create fresh)
python3.11 -m venv venv
source venv/bin/activate        # Linux/macOS
# venv\Scripts\activate         # Windows
pip install -r requirements.txt

# Configure
cp .env.examples .env
# Edit .env with your credentials

# Health check
python healthcheck.py

# Paper mode (simulation, no real orders)
python main.py --paper

# Live mode (real funds — run paper first!)
python main.py --live
```

**On Replit:** Set up a workflow with command:
```
bash -c "cd polymarket_bot && python -m venv venv && source venv/bin/activate && pip install -r requirements.txt -q && python main.py --paper"
```

## Configuration Reference (`.env`)

### Critical credentials (live mode only)
| Variable | Description |
|----------|-------------|
| `POLYMARKET_PRIVATE_KEY` | Wallet private key for signing orders (0x...) |
| `POLYMARKET_FUNDER` | Wallet address holding USDC on Polygon |
| `POLYMARKET_SIGNATURE_TYPE` | `1` = EIP-712 Gnosis Safe proxy · `0` = EOA direct |

### Strategy & markets
| Variable | Default | Notes |
|----------|---------|-------|
| `STRATEGY` | `latency_arb` | `latency_arb` / `dump_hedge` / `both` |
| `MARKETS` | `btc,eth,sol` | Comma-separated: `btc`, `eth`, `sol`, `xrp` |
| `TRADE_WINDOW_MINUTES` | `5` | Must be `5` or `15` |
| `PAPER_MODE` | `true` | `false` for live trading |
| `PAPER_STARTING_BALANCE` | `1000.0` | Virtual balance for simulation |

### Edge detection (latency arb)
| Variable | Default | Notes |
|----------|---------|-------|
| `EDGE_LAG_WINDOW_SECONDS` | `2.7` | Measured Binance→Polymarket lag. Don't go below 2.0 |
| `EDGE_MIN_EDGE_THRESHOLD` | `0.04` | Minimum probability advantage to trade |
| `EDGE_COOLDOWN_SECONDS` | `5.0` | Per-asset cooldown between signals |
| `EDGE_MIN_ENTRY_PRICE` | `0.38` | Minimum token price to enter (avoids already-decided markets) |
| `EDGE_MAX_ENTRY_PRICE` | `0.62` | Maximum token price to enter |
| `EDGE_MIN_FAIR_VALUE_STRENGTH` | `0.05` | Model must have ≥55% conviction |
| `EDGE_MIN_MARKET_LIQUIDITY` | `1000.0` | Minimum USDC liquidity in market |
| `EDGE_MIN_SECONDS_REMAINING` | auto (20% of window) | 60s for 5-min, 180s for 15-min |

### Dump hedge
| Variable | Default | Notes |
|----------|---------|-------|
| `DH_SUM_TARGET` | `0.95` | Max combined YES+NO ask to enter (≤ this = signal) |
| `DH_MIN_DISCOUNT` | `0.03` | Minimum locked discount per share (covers fees) |
| `DH_FIXED_BET_USDC` | `50.0` | Total USDC per trade, split ~50/50 across legs. Min $2 |
| `DH_EARLY_EXIT_PROFIT_FRACTION` | `0.70` | Exit early when 70% of locked profit is realised |
| `DH_TIMEOUT_SECONDS` | auto (90% of window) | Force-close after this many seconds |
| `DH_COOLDOWN_SECONDS` | `30.0` | Per-asset cooldown between DH signals |

### Risk management
| Variable | Default | Notes |
|----------|---------|-------|
| `KELLY_ENABLED` | `false` | `true` = Kelly formula · `false` = fixed bet |
| `RISK_FIXED_BET_USDC` | `0.0` | Fixed USDC per trade. 0.0 = use Kelly |
| `RISK_KELLY_FRACTION` | `0.5` | Fractional Kelly multiplier (0.5 = half-Kelly) |
| `RISK_MAX_POSITION_FRACTION` | `0.08` | Max single position as fraction of balance |
| `RISK_MAX_CONCURRENT_POSITIONS` | `3` | Max open positions at once |
| `RISK_DAILY_LOSS_LIMIT` | `0.20` | Pause trading after 20% daily loss |
| `RISK_TOTAL_DRAWDOWN_KILL` | `0.40` | Kill switch at 40% total drawdown (requires manual reset) |

### Exit thresholds (latency arb positions)
| Variable | Default | Notes |
|----------|---------|-------|
| `TAKE_PROFIT_PRICE` | `0.72` | Exit when token price hits 72¢ |
| `TAKE_PROFIT_PNL` | `0.12` | Exit when unrealised gain reaches +12% |
| `STOP_LOSS_PNL` | `-0.20` | Cut when down -20% on position cost |
| `NEAR_WIN_PRICE` | `0.92` | Exit at 92¢ (near-certain YES resolution) |
| `NEAR_LOSS_PRICE` | `0.08` | Cut at 8¢ (near-certain NO resolution) |
| `POSITION_TIMEOUT_SECONDS` | auto (window × 60) | Force-close before market resolves |

### Notifications
| Variable | Default | Notes |
|----------|---------|-------|
| `TELEGRAM_ENABLED` | `true` | Enable/disable Telegram alerts |
| `TELEGRAM_BOT_TOKEN` | — | From @BotFather on Telegram |
| `TELEGRAM_CHAT_ID` | — | Your chat/group ID |
| `OPENCLAW_ENABLED` | `false` | Enable AI agent integration |
| `OPENCLAW_API_KEY` | — | OpenClaw API key |
| `OPENCLAW_AGENT_ID` | — | OpenClaw agent ID |

### Logging & dashboard
| Variable | Default | Notes |
|----------|---------|-------|
| `LOG_LEVEL` | `INFO` | `DEBUG` / `INFO` / `WARNING` |
| `LOG_FILE` | `polymarket_bot.log` | Rotating log (10 MB × 5 backups) |
| `DASHBOARD_LOG_LINES` | `20` | Recent log lines shown in terminal dashboard |

## Risk Management State Machine

```
         ACTIVE
        /  |   \
       /   |    \
PAUSED/    |     \KILLED (permanent — requires reset_kill_switch(confirm=True))
      \    |     /
       \   |    /
        DAILY_HALT (resets midnight UTC)
```

- **PAUSED**: manual pause via OpenClaw command
- **DAILY_HALT**: daily loss limit breached — resumes next UTC day
- **KILLED**: total drawdown kill triggered — requires explicit manual reset

## Market Discovery

Markets are found using two strategies in sequence:
1. **Parallel slug probe** — probes 4 slug candidates concurrently (`{asset}-updown-5m-{unix_ts}`) — 1 HTTP round-trip
2. **Gamma tag search** — fallback using `data-api.polymarket.com` tag filter if slug probe fails

## Typical Debugging

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Failed to receive BTC price within 35s` | Binance WS blocked (US IP, firewall) | Use a VPS in Singapore; check ports 9443/443 |
| `PM WS: Connection rejected (403/451)` | Polymarket WS geo-blocked | Bot falls back to REST automatically; run on non-US VPS |
| `Kill switch active` | Total drawdown limit hit | Check `RISK_TOTAL_DRAWDOWN_KILL` — requires manual reset via code |
| No trades firing | Edge threshold too high, or liquidity too low | Lower `EDGE_MIN_EDGE_THRESHOLD`, lower `EDGE_MIN_MARKET_LIQUIDITY` |
| Order rejected by CLOB | Bet size < $1.00 minimum | Don't set `DH_FIXED_BET_USDC` below 2 |
| Dashboard garbled on Windows | CMD/PowerShell encoding issue | Use Windows Terminal; `sys.stdout.reconfigure(encoding="utf-8")` is already applied |

## Common Modifications

### Change strategy to dump hedge only (no Binance needed)
```env
STRATEGY=dump_hedge
DH_SUM_TARGET=0.97
DH_MIN_DISCOUNT=0.02
DH_FIXED_BET_USDC=5
```

### Aggressive latency arb (more signals, more risk)
```env
STRATEGY=latency_arb
EDGE_MIN_EDGE_THRESHOLD=0.02
EDGE_COOLDOWN_SECONDS=1.0
EDGE_MIN_FAIR_VALUE_STRENGTH=0.03
```

### Conservative fixed-bet mode
```env
KELLY_ENABLED=false
RISK_FIXED_BET_USDC=2.0
RISK_MAX_CONCURRENT_POSITIONS=1
RISK_DAILY_LOSS_LIMIT=0.10
RISK_TOTAL_DRAWDOWN_KILL=0.20
```

### Enable Telegram alerts
```env
TELEGRAM_ENABLED=true
TELEGRAM_BOT_TOKEN=<token from @BotFather>
TELEGRAM_CHAT_ID=<your chat ID>
```

## Key Implementation Notes

- **Never use `console.log` / `print()` in live code** — use `logger = get_logger(__name__)` from `utils/logger.py`. The Rich dashboard redirects stdout; raw prints break the display.
- **Position lock per asset** — `_asset_open_position[asset]` blocks new signals for that asset until the current position closes. Do not bypass this.
- **FAK orders only** — all live orders are Fill-And-Kill. There are no resting limit orders in the book.
- **Paper mode is safe** — `PAPER_MODE=true` simulates fills without touching real funds. Always validate here first.
- **The main loop runs at 20 Hz** (`LOOP_INTERVAL_SECONDS = 0.05`). Position exit checks run every 0.5s. Dashboard refreshes every 2s.
- **`config.py` must be imported before any other module** — it calls `load_dotenv()` at module level.
- **Adding a new asset**: add it to `ASSET_CONFIG` in `edge_detector.py`, add its Binance symbol to `_ASSET_SYMBOLS` in `main.py`, and add it to `_VALID_MARKETS` in `config.py`.

## Secrets Required (for Live Mode)

Manage via Replit Secrets (never commit to code):
- `POLYMARKET_PRIVATE_KEY` — wallet private key
- `POLYMARKET_FUNDER` — wallet address
- `TELEGRAM_BOT_TOKEN` — (optional) Telegram bot token
- `OPENCLAW_API_KEY` — (optional) OpenClaw API key
