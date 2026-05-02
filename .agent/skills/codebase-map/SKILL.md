---
name: project-codebase-map
description: >
  A navigational map of this repository. Load this skill when looking for where
  existing code lives, deciding where to place new code, checking what features
  or routes exist, or understanding how the project is organised. Also load when
  asked "where is X" or "how does Y connect to Z" about this codebase.
---

# CODEBASE.md
> A navigational map of this repository for AI agents and contributors.
> Update this file whenever a new top-level directory, route, or major module is added.

---

## Stack

| Layer | Technology |
|---|---|
| C++ Core | C++20, Boost.Asio/io_uring, simdjson |
| Frontend Framework | Next.js 14+ App Router |
| Frontend Language | TypeScript |
| Styling | TailwindCSS + shadcn/ui |
| State Management | Zustand + TanStack Query |
| Database | PostgreSQL (History) + Redis (Live State) |
| Auth | WebAuthn |

---

## Repository Structure

```
/
├── trading-core/          → C++ trading engine source
├── frontend/              → Next.js 14+ web application
├── public/                → Static assets
├── .agent/
│   └── skills/
│       ├── agents-rules/SKILL.md
│       ├── architecture/SKILL.md
│       ├── codebase-map/SKILL.md    ← this file
│       ├── decisions-log/SKILL.md
│       ├── philosophy/SKILL.md
│       ├── frontend/SKILL.md
│       └── backend/SKILL.md
```

---

## C++ Core Structure

```
trading-core/
├── src/
│   ├── feeds/               → Binance WS, Polymarket WS + REST
│   ├── model/               → FairValueModel (Sigmoid)
│   ├── signals/             → LatencyArbDetector, DumpHedgeDetector
│   ├── risk/                → RiskManager failsafes
│   ├── exec/                → OrderRouter, EIP712Signer
│   ├── state/               → In-memory state, snapshot storage
│   ├── control/             → gRPC Control Plane
│   └── main.cpp             → Wiring, lifecycle
├── tests/                   → GoogleTest, deterministic replay
├── CMakeLists.txt
└── conanfile.txt
```

---

## Frontend Structure

```
frontend/
├── src/
│   ├── app/                    → Routes and API handlers
│   │   ├── (routes)/           → /dashboard, /risk, /history, etc.
│   │   └── api/                → API Gateway endpoints (Node.js)
│   ├── components/             → All React components
│   │   ├── ui/                 → shadcn/ui primitives
│   │   └── ...                 → Feature components
│   ├── store/                  → Zustand global state
│   ├── hooks/                  → Shared hooks (e.g., SSE, WebAuthn)
│   ├── lib/                    → Pure utility functions
│   └── types/                  → Shared TypeScript types
```

---

## Where to Find Things

| I need to... | Look in... |
|---|---|
| Update a trading strategy | `trading-core/src/signals/` |
| Change execution logic / EIP-712 | `trading-core/src/exec/` |
| Change risk limits | `trading-core/src/risk/RiskManager.cpp` |
| Add a dashboard widget | `frontend/src/app/(routes)/dashboard/` |
| Add a new intent API | `frontend/src/app/api/` |
| Update the auth flow | `frontend/src/hooks/useWebAuthn.ts` or API routes |
