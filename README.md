# Polymarket Arbitrage Trading Bot

A high-performance arbitrage trading bot designed to exploit structural and latency-based edges on Polymarket.

## Architecture

This project strictly adheres to a **3-tier architecture**, explicitly segregating the trading core from the control plane to ensure maximum performance and absolute security.

1. **C++ Trading Core (`trading-core/`)**
   - **Performance:** Zero-allocation on the hot path. Built in C++20.
   - **I/O:** Asynchronous WebSockets using `Boost.Beast` and `kqueue`/`epoll`.
   - **JSON:** Blazing fast parsing using `simdjson`.
   - **Responsibility:** Ingests Binance and Polymarket WebSocket feeds, runs the time-aware sigmoid pricing model, validates intents against the 4-layer risk engine, and executes EIP-712 signed orders.
   - *Crucially: The C++ core is never directly exposed to the internet.*

2. **Node.js API Gateway (`frontend/src/app/api`)**
   - **Security:** WebAuthn-gated endpoints. Every state-changing intent requires a fresh passkey challenge.
   - **Logging:** Append-only PostgreSQL audit logging for all intents *before* passing them to the core.
   - **Communication:** Sends validated intents to the C++ core via gRPC.

3. **Next.js Dashboard (`frontend/`)**
   - **Stack:** Next.js 14+ App Router, TailwindCSS, shadcn/ui.
   - **State:** Zustand (global), TanStack Query (server state).
   - **Real-time:** Receives live state (PnL, current edge, system health) from the gateway via Server-Sent Events (SSE).

## Authentication

By default, the dashboard is protected. An initial super user is seeded into the database for local development.

- **Email:** `admin@tradebot.local`
- **Password:** `SuperSecretPassword123!`

> [!NOTE]
> If you need to change these default credentials, update them in `frontend/prisma/seed.ts` and re-run `npx prisma db seed` from the `frontend` directory.


## Building the C++ Core

The C++ core is designed to be cross-platform for development but optimized for Linux in production. It uses `CMake` and `Conan` for dependency management. It builds directly into the `build/` directory at the repository root.

```bash
./build.sh
```
This script will install Conan if needed, download the required dependencies (`boost`, `simdjson`, `openssl`, `spdlog`), and compile the binary.

## Project Documentation & Guidelines

All operational rules, coding conventions, and architectural decisions are meticulously documented and enforced by AI agents.

- **[SPECS.md](./SPECS.md):** The master technical specification.
- **[Agent Skills](./.agent/skills/):**
  - [`architecture`](./.agent/skills/architecture/SKILL.md): Core philosophy and layer separation rules.
  - [`backend`](./.agent/skills/backend/SKILL.md): C++ and API Gateway conventions (zero-allocation, intent validation).
  - [`frontend`](./.agent/skills/frontend/SKILL.md): React component tiers, state management, and strict WebAuthn rules.
  - [`codebase-map`](./.agent/skills/codebase-map/SKILL.md): Directory navigation.
  - [`decisions-log`](./.agent/skills/decisions-log/SKILL.md): Immutable architectural decisions (e.g., C++ separation).

## Risk Management (4-Layer Engine)

1. **Position Fraction Cap:** Max % of bankroll per trade.
2. **Concurrent Position Limit:** Max total open positions.
3. **Daily Soft-Halt:** Pause new entries if daily budget is exceeded.
4. **Drawdown Hard-Kill:** Complete halt if peak-to-trough equity drops beyond threshold (requires WebAuthn reset).
