---
name: project-backend
description: >
  Backend-specific conventions, patterns, and rules for this project. Load this
  skill when working on any API endpoint, C++ trading core module, database query, auth
  logic, or server-side business rule.
---

# BACKEND.md
### Backend Conventions & Patterns

---

## Stack

| Tool | Purpose |
|---|---|
| C++20 | Trading Core Language |
| Node.js / Next.js | API Gateway Language |
| PostgreSQL | Persistent Database |
| Redis | Hot State Mirror |
| WebAuthn | Authentication |
| gRPC | Inter-process Communication |

---

## §1  C++ Trading Core Design

### 1.1  Zero-Allocation Hot Paths

The hot trading path (from Binance WS frame to signal decision to order submission) must not allocate memory.

```cpp
// ❌ No std::string copies or dynamic vectors
void process_tick(std::string symbol) {
    std::vector<int> data;
    data.push_back(1);
}

// ✅ Pre-allocated buffers, string_view
void process_tick(std::string_view symbol) {
    // Modify pre-allocated ring buffer
}
```

### 1.2  Intent Validation

The C++ core receives intents via gRPC, not direct order commands. The core validates the intent against current risk parameters before executing.

```cpp
// The frontend says "Set risk multiplier to 2x"
// The Core evaluates if this breaches Layer 4 limits first.
```

### 1.3  Asynchronous I/O

All networking (WebSockets, REST) uses `Boost.Asio` or `io_uring` with C++20 coroutines to avoid blocking the single execution thread.

---

## §2  API Gateway (Node.js) Design

### 2.1  The Gateway Contract

Route handlers do exactly three things. No more.

```ts
// 1. Authenticate (WebAuthn session)
// 2. Validate intent payload
// 3. Log to audit database
// 4. Delegate to gRPC C++ client
```

### 2.2  Auth Enforcement

**Every state-changing endpoint enforces a fresh WebAuthn challenge.**

```ts
// ❌ This is never acceptable
router.post('/disable-kill-switch', async (req, res) => {
    // Direct change without re-auth
});

// ✅ Fresh WebAuthn challenge required
router.post('/disable-kill-switch', requireFreshAuthChallenge, async (req, res) => {
    // Proceed
});
```

### 2.3  Audit Logging First

Never send an intent to the C++ core without first logging it to PostgreSQL. This creates an append-only audit trail that cannot be bypassed.

---

## §3  Data Storage

### 3.1  PostgreSQL (History & Audit)

Used strictly for historical data, trade logs, and audit trails. The C++ core does not read from Postgres in the hot path.

### 3.2  Redis (Hot State)

Used for fast pub/sub state mirroring. The C++ core pushes updates (e.g., PnL, current edge) to Redis, and the Next.js frontend reads them via Server-Sent Events (SSE) from the Node API gateway.

---

## §4  Secrets Management

| Secret                 | Location                      | Access |
| ---------------------- | ----------------------------- | ------------------ |
| Polymarket private key | C++ core process only         | Must NEVER touch Node.js |
| Binance API key        | C++ core, **read-only scope** | Must NEVER touch Node.js |
| Database credentials   | API gateway only              | Node.js only |
| Session signing key    | API gateway only              | Node.js only |

---

## §5  Agent Reminders

Before submitting any backend change:

- [ ] Does every new state-changing endpoint use a WebAuthn challenge?
- [ ] Is the C++ hot path completely free of `new`, `malloc`, or `std::string` copies?
- [ ] Are secrets isolated properly between the Node and C++ tiers?
- [ ] Does the new feature log to the audit database before execution?
