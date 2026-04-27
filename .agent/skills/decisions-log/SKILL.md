---
name: project-decisions-log
description: >
  A record of significant architectural and technical decisions — what was chosen,
  what was rejected, and why. Load this skill before making any architectural change,
  choosing a new library, or changing an existing pattern. AI agents must treat these
  as immutable constraints unless a human explicitly reopens a decision. To propose
  a new decision, output an AGENT FLAG and wait for human approval.
---

# DECISIONS.md
> Architectural decision log.
> AI agents: treat these as immutable constraints unless a human explicitly reopens a decision.

---

## Template

```
## D-{number}: {Title}
Status: Accepted | Superseded by D-{n} | Under Review
Date: YYYY-MM-DD

**Decision:** What was chosen.
**Context:** Why the decision was needed.
**Rationale:** Why this choice over the alternatives.
**Consequences:** What this means going forward.
**Rejected:** What was considered and why it lost.
```

---

## D-001: Separation of C++ Core and WebAuthn Control Plane

**Status:** Accepted
**Date:** 2026-04-27

**Decision:** Isolate all trading execution and signal logic in a zero-allocation C++20 core, communicating via gRPC with a Next.js (Node.js) WebAuthn-gated API gateway.

**Context:** The Python implementation struggled with consistent low-latency execution and had a dashboard/execution environment coupled in the same process, increasing security risk if exposed to the internet.

**Rationale:** C++ provides the absolute lowest tail latency for Strategy A (Latency Arb), maximizing the edge captured before oracle updates. Offloading the UI and API layer to Next.js ensures the C++ core is never directly exposed to HTTP traffic or the public internet. WebAuthn ensures unphishable control over critical bot state.

**Consequences:**
- The trading core must be written in C++20 and use `simdjson` + `Boost.Asio`/`io_uring` for optimal performance.
- Polymarket EIP-712 order signing must be implemented in C++ or via Rust FFI.
- The Node.js gateway handles all history, UI state, and user interactions, passing intent down to the C++ core via gRPC.

**Rejected:** Pure Python (too slow for tail latency), Rust (user preferred C++ for the core), Hybrid Python/C++ (decided against middle paths to simplify the stack to just pure C++ for execution and TS for UI).

---

## Adding a New Decision

Copy the template at the top. Number sequentially. Keep it concise.

**If you are an AI agent:** never make an architectural decision unilaterally.
Instead, output this and wait:

```
AGENT FLAG: Potential new decision needed
Choice: [describe the decision that needs to be made]
Options: [list the realistic options]
My recommendation: [which option and why]
Impact: [what will need to change if this is accepted]
```
