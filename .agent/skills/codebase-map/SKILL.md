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

<!-- ================================================================
  SETUP INSTRUCTIONS (remove this block when done)
  1. Replace {{PROJECT_NAME}} with your project name
  2. Fill in the Stack table with your actual technologies
  3. Replace the directory trees with your actual structure
  4. Fill in the Route/Feature map table
  5. Update the "Where to Find Things" table
  6. Delete all {{ }} placeholders and these comments
================================================================ -->

---

## Stack

| Layer | Technology |
|---|---|
| {{Frontend Framework}} | {{e.g. Next.js App Router}} |
| {{Frontend Language}} | {{e.g. TypeScript (strict)}} |
| {{Styling}} | {{e.g. TailwindCSS}} |
| {{State Management}} | {{e.g. Zustand + React Query}} |
| {{Backend Framework}} | {{e.g. FastAPI / Express / None}} |
| {{Database}} | {{e.g. PostgreSQL / SQLite / None}} |
| {{Auth}} | {{e.g. JWT / NextAuth / Clerk}} |
| {{Testing}} | {{e.g. Vitest + Playwright}} |

---

## Repository Structure

```
/
├── {{src/ or app/}}       → Frontend source (see below)
├── {{backend/ or api/}}   → Backend source (see below) [remove if not applicable]
├── public/                → Static assets
├── .env.example           → Environment template — never commit .env
├── {{config files}}       → Do not edit without approval
└── .agent/
    └── skills/
        ├── agents-rules/SKILL.md
        ├── architecture/SKILL.md
        ├── codebase-map/SKILL.md    ← this file
        ├── decisions-log/SKILL.md
        ├── philosophy/SKILL.md
        ├── frontend/SKILL.md        [if applicable]
        └── backend/SKILL.md         [if applicable]
```

---

## Frontend Structure
<!-- Remove this section if no frontend -->

```
{{src/}}
├── {{app/ or pages/}}          → Routes and layouts
│   ├── layout.tsx               → Root shell, providers
│   ├── globals.css              → Design tokens, global styles
│   └── {{(routes)/}}           → Content pages
│       └── {{route}}/page.tsx
│
├── {{components/}}              → All React components
│   ├── {{ui/}}                  → Primitives: Button, Input, Badge, etc.
│   └── {{feature components}}
│
├── {{context/ or store/}}       → Global state providers
├── {{hooks/}}                   → Shared hooks
├── {{lib/ or utils/}}           → Pure utility functions
└── {{types/}}                   → Shared TypeScript types
```

---

## Backend Structure
<!-- Remove this section if no backend -->

```
{{backend/}}
├── main.py (or index.ts)        → Entrypoint, router registration
├── {{routers/ or routes/}}      → HTTP handlers (thin — validate + delegate only)
├── {{services/}}                → Business logic (pure where possible)
├── {{models/ or schemas/}}      → Data shapes, Pydantic/Zod models
├── {{db/ or database/}}         → DB connection, query helpers, migrations
└── {{auth/}}                    → Auth dependencies, JWT, session helpers
```

---

## Route → Feature Map

<!-- Add a row for each page/route in your app -->

| Route | Component / Module | Backend Endpoint(s) | Auth Required |
|---|---|---|---|
| `{{/route}}` | `{{ComponentName}}` | `{{GET /api/resource}}` | {{Yes / No}} |
| `{{/route}}` | `{{ComponentName}}` | `{{POST /api/resource}}` | {{Yes / No}} |

---

## Key Conventions

<!-- Document the 3-5 most important conventions agents must follow in this project -->

### {{Convention 1 — e.g. Every page uses PageContainer}}
```tsx
// Example showing the convention
```

### {{Convention 2 — e.g. Every protected endpoint uses auth dependency}}
```python
# Example showing the convention
```

### {{Convention 3 — e.g. All user data is scoped by user_id}}
```python
# Example showing the convention
```

---

## Path Aliases
<!-- Update or remove if not applicable -->

| Alias | Resolves to |
|---|---|
| `@/components/*` | `src/components/*` |
| `@/hooks/*` | `src/hooks/*` |
| `@/lib/*` | `src/lib/*` |
| `@/types/*` | `src/types/*` |
| `{{add yours}}` | `{{path}}` |

---

## Where to Find Things

| I need to... | Look in... |
|---|---|
| Add a new page | `{{src/app/(routes)/name/page.tsx}}` |
| Add a UI primitive | `{{src/components/ui/}}` |
| Change a design token | `{{src/app/globals.css}}` |
| Add global state | `{{src/context/ or src/store/}}` |
| Add a shared hook | `{{src/hooks/}}` |
| Add a pure utility | `{{src/lib/}}` |
| Add an API endpoint | `{{backend/routers/}}` |
| Add business logic | `{{backend/services/}}` |
| Add a data model | `{{backend/models/}}` |
| Change the DB schema | `{{backend/database/schema.sql}}` |
| Find the auth flow | `{{backend/auth/ + src/context/AuthContext}}` |
