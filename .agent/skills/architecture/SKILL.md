---
name: project-architecture
description: >
  Core architectural philosophy and structural rules for this codebase. Load
  this skill when working on any structural decision, adding new modules,
  designing new features, or when unsure where code belongs. Also load when
  asked about conventions, patterns, or "how we do things here".
---

# ARCHITECTURE.md
### {{PROJECT_NAME}} — Architectural Guide

<!-- ================================================================
  SETUP INSTRUCTIONS (remove this block when done)
  1. Replace {{PROJECT_NAME}} everywhere
  2. Fill in §2 Directory Architecture with your actual structure
  3. Keep only the stack sections that apply (frontend / backend / both)
  4. Fill in the Responsibility Boundaries table for your entities
  5. Update the agent placement table in §6
  6. Delete all {{ }} placeholders and these comments
================================================================ -->

---

## §1  Core Philosophy

> Architecture is a communication medium — it tells humans and AI agents where things live, why they live there, and how they should talk to each other.

Good architecture is not about enforcing rigid rules. It is about establishing clear **gravitational centres** so that every file and every function has an obvious home. When a developer — or an AI agent — needs to add something new, the structure should make the right choice feel natural and the wrong choice feel awkward.

> **The Prime Directive:** Each piece of code should do one thing, know where it belongs, and ask for everything else it needs. Surprise is the enemy of maintainability.

### 1.1  Three Questions Every Module Must Answer

| # | Question | If you cannot answer clearly... |
|---|----------|---------------------------------|
| 1 | What is my single responsibility? | Service, transform, render, route, validate — pick one. |
| 2 | Who owns me? | A feature, a shared service, a UI system, a layout. |
| 3 | What do I depend on? | Dependencies flow inward only. Never import up the tree. |

---

## §2  Directory Architecture

<!-- Replace this section with your actual directory tree.
     Keep the conventions note — update it to match your project. -->

> The file tree is the first thing an agent reads. Make it a map, not a maze.

**Convention:** {{Describe your main structural convention here. E.g.: "Features own their world. Shared code lives in well-named shared zones. No feature imports from another feature — they communicate through services only."}}

```
<!-- Paste your actual directory tree here -->
src/
  ...
```

---

## §3  Frontend Architecture
<!-- Remove this section entirely if the project has no frontend -->

### 3.1  Component Hierarchy

```
Design Tokens
    ↓
UI Primitives          ← No business logic, no data fetching
    ↓
Shared Layout          ← Structural shells, slots only
    ↓
Feature Components     ← May use hooks, may read context
    ↓
Page Components        ← Orchestration only, thin
```

### 3.2  Data Flow

```
Page Component
    ↓  calls
Service / Hook (data fetching)
    ↓  returns
Typed response
    ↓  passed to
Feature Component
    ↓  reads global state from
Context / Store
```

### 3.3  State Layers

| Layer | Tool | Owns |
|---|---|---|
| Server state | {{e.g. React Query / SWR}} | Remote data, caching, refetch |
| Global UI state | {{e.g. Zustand / Context}} | Auth, theme, user prefs |
| Local UI state | `useState` | Component-scoped ephemeral state |

### 3.4  Frontend Rules

- UI primitives have **zero business logic** and **zero data fetching**.
- Feature components may read context but must **not** call APIs directly.
- Data fetching lives in dedicated hooks or page components.
- Design tokens are the **single source of truth** for colour, spacing, and type scale.
- All interactive components must be keyboard-accessible with aria labels.

---

## §4  Backend Architecture
<!-- Remove this section entirely if the project has no backend -->

### 4.1  Layer Responsibilities

| Layer | Lives in | Responsibility |
|---|---|---|
| Router / Handler | `{{routers/}}` | Validate input, authenticate, delegate. Nothing more. |
| Service | `{{services/}}` | Business rules, orchestration, pure logic. |
| Repository / DB | `{{db/ or models/}}` | Data access only. No business logic. |
| Model / Schema | `{{models/}}` | Data shape. No logic. |

### 4.2  Handler Contract

Every route handler does exactly three things:

```python
# 1. Authenticate / authorise
# 2. Validate input (Pydantic / Zod handles this)
# 3. Delegate to a service function and return the result

@router.post("/invoices")
async def create_invoice(body: InvoiceCreate, user=Depends(get_current_user)):
    return await invoice_service.create(body, user.id)
```

### 4.3  Service Contract

Service functions are pure where possible. They take typed data, return typed data, and isolate side effects at their boundary.

```python
# ✅ Pure core
def calculate_total(items: list[LineItem], tax_rate: float) -> InvoiceSummary:
    subtotal = sum(i.price * i.qty for i in items)
    return InvoiceSummary(subtotal=subtotal, tax=subtotal * tax_rate)

# Side effects quarantined at the edge
async def create_invoice(body: InvoiceCreate, user_id: str) -> Invoice:
    total = calculate_total(body.items, body.tax_rate)   # pure
    return await db.invoices.insert({**body.dict(), "total": total, "user_id": user_id})
```

### 4.4  Auth Pattern

```python
# Every protected endpoint uses the auth dependency
@router.get("/resource")
async def get_resource(user = Depends(get_current_user)):
    ...

# User-owned data is always scoped by user_id — never return all rows
items = await db.get_all(where={"user_id": user.id})
```

---

## §5  Responsibility Boundaries

> Every class, hook, and service answers one question. When it starts answering two, split it.

### 5.1  Boundary Map

<!-- Customise this table for your actual entities -->

| Entity | Owns | Must NOT own |
|---|---|---|
| **Page Component** | Route, data orchestration | Business logic, direct API calls |
| **Feature Component** | Feature rendering, local state | Routing, cross-feature data, API calls |
| **UI Primitive** | Visual presentation, a11y | Data fetching, business rules, external state |
| **Custom Hook** | Stateful logic, side-effect lifecycle | JSX, direct DOM manipulation |
| **Service Function** | Business rule, data transform | HTTP concerns, rendering, component state |
| **Store / Context** | Shared runtime state | Fetching, business logic |
| **Route Handler** | Auth, validation, delegation | Business logic |
| **{{Add your entities}}** | {{Owns}} | {{Must not own}} |

### 5.2  The Dependency Rule

```
Dependencies only flow inward. No layer imports from a layer further out.

  UI / Presentation
      ↓  imports
  Application (hooks, stores)
      ↓  imports
  Domain (services, pure functions, types)
      ↓  imports
  Infrastructure (HTTP, DB, external SDKs)

  ← nothing imports upward →
```

---

## §6  Notes for AI Agents

### 6.1  Where to Put New Code

<!-- Update this table to match your project structure -->

| What you're building | Where it goes | Constraint |
|---|---|---|
| New UI primitive | `{{src/components/ui/}}` | No business logic. No data fetching. |
| New page | `{{src/app/route/page.tsx}}` | Thin. Orchestration only. |
| New feature component | `{{src/components/}}` | Single responsibility. |
| New shared hook | `{{src/hooks/}}` | No JSX. State and handlers only. |
| New API endpoint | `{{backend/routers/}}` | Validate, auth, delegate. |
| New business logic | `{{backend/services/}}` | Pure function. Typed in/out. |
| New data model | `{{backend/models/}}` | Shape only. No logic. |

### 6.2  The Agent Mantra

> **"Find it before you build it. Build it small. Put it in the right place. Export it cleanly. Leave no mysteries."**

---

*Architecture is a living document. Revisit it when patterns break down.*
