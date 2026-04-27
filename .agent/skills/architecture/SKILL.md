---
name: project-architecture
description: >
  Core architectural philosophy and structural rules for this codebase. Load
  this skill when working on any structural decision, adding new modules,
  designing new features, or when unsure where code belongs. Also load when
  asked about conventions, patterns, or "how we do things here".
---

# ARCHITECTURE.md
### Polymarket Arbitrage Bot — Architectural Guide

---

## §1  Core Philosophy

> Architecture is a communication medium — it tells humans and AI agents where things live, why they live there, and how they should talk to each other.

The system is split into three tiers. The critical design rule: **the C++ trading core is never directly exposed to the internet**. The web layer can fail, get attacked, or restart without touching the trading engine.

### 1.1  Three Questions Every Module Must Answer

| # | Question | If you cannot answer clearly... |
|---|----------|---------------------------------|
| 1 | What is my single responsibility? | Feed, model, exec, router, store, intent validation — pick one. |
| 2 | Who owns me? | The C++ Core, the API Gateway, or the Frontend. |
| 3 | What do I depend on? | Dependencies flow inward only. Never import up the tree. |

---

## §2  Directory Architecture

> The file tree is the first thing an agent reads. Make it a map, not a maze.

**Convention:** Strict isolation between the C++ trading core and the Next.js control plane.

```
/
├── trading-core/
│   └── src/                 # C++20 trading engine
├── frontend/                # Next.js 14+ App Router (Control Plane)
```

---

## §3  Frontend & API Gateway Architecture

### 3.1  Component Hierarchy

```
Design Tokens (Tailwind + shadcn/ui)
    ↓
UI Primitives          ← No business logic, no data fetching
    ↓
Shared Layout          ← Structural shells, slots only
    ↓
Feature Components     ← May use Zustand/TanStack Query
    ↓
Page Components        ← Orchestration, RSC (React Server Components)
```

### 3.2  Data Flow

```
Page Component
    ↓  calls
Server Action / API Route
    ↓  logs to
Audit Log (PostgreSQL)
    ↓  communicates via gRPC to
C++ Trading Core
```

### 3.3  State Layers

| Layer | Tool | Owns |
|---|---|---|
| Server state | TanStack Query | Remote data, history, config |
| Live state | SSE + Redis | Fast-moving tick data, PnL updates |
| Global UI state | Zustand | Theme, UI preferences |
| Local UI state | `useState` | Component-scoped ephemeral state |

---

## §4  C++ Trading Core Architecture

### 4.1  Layer Responsibilities

| Layer | Lives in | Responsibility |
|---|---|---|
| Control Plane | `control/` | gRPC server, validate intents from the frontend. |
| State Store | `state/` | In-memory state, snapshots, publishes to Redis. |
| Signals | `signals/` | Strategy logic (Latency Arb, Dump Hedge). |
| Risk | `risk/` | 4-layer failsafe evaluation. |
| Execution | `exec/` | EIP-712 signing, Order routing to Polymarket CLOB. |
| Feeds | `feeds/` | Binance WS, Polymarket WS + REST. |

### 4.2  Memory & Performance Contract

The C++ core must guarantee zero allocations in the hot path.

```cpp
// 1. Pre-allocate everything on startup
// 2. Use object pools and ring buffers
// 3. No std::string copies, use string_view
// 4. Use simdjson for parsing
```

---

## §5  Responsibility Boundaries

> Every class, hook, and service answers one question. When it starts answering two, split it.

### 5.1  Boundary Map

| Entity | Owns | Must NOT own |
|---|---|---|
| **C++ Core** | Signal detection, execution, risk enforcement | HTTP APIs, user auth, persistent history |
| **API Gateway** | Auth (WebAuthn), audit logging, intent validation | Trading logic, direct exchange connections |
| **Frontend UI** | Displaying state, capturing intents | Business rules, secrets |

### 5.2  The Dependency Rule

```
Dependencies only flow inward. No layer imports from a layer further out.

   C++ Trading Core
       ↑  gRPC intents
   API Gateway (Node.js)
       ↑  HTTPS (WebAuthn)
   Next.js Frontend

   ← nothing imports downward →
```

---

## §6  Notes for AI Agents

### 6.1  Where to Put New Code

| What you're building | Where it goes | Constraint |
|---|---|---|
| New strategy | `trading-core/src/signals/` | Pure C++, event-driven. |
| New feed | `trading-core/src/feeds/` | Zero allocation, asynchronous. |
| New UI dashboard | `frontend/src/app/` | RSC, strict auth checks. |
| New intent/action | `frontend/src/app/api/` | Must log to audit before sending gRPC. |

### 6.2  The Agent Mantra

> **"Find it before you build it. Build it small. Put it in the right place. Export it cleanly. Leave no mysteries."**

---

*Architecture is a living document. Revisit it when patterns break down.*
