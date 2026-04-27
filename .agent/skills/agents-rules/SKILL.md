---
name: project-agents-rules
description: >
  Operational rules and hard constraints for all AI agents working in this
  codebase. Load this skill at the start of every session, or whenever working
  on any task that involves writing, editing, or deleting code. This is the
  agent's constitution — it overrides everything else except explicit human
  instructions.
---

# AGENTS.md
> Operational rules for all AI agents working in this codebase.
> Read this file in full before writing, editing, or deleting anything.

<!-- ================================================================
  SETUP INSTRUCTIONS (remove this block when done)
  1. Replace {{PROJECT_NAME}} with your project name
  2. Replace {{STACK}} with your stack e.g. "Next.js + FastAPI"
  3. Replace {{MODELS}} with models in use e.g. "Claude Sonnet, Gemini"
  4. Fill in the project-specific rules section at the bottom
  5. Delete all {{ }} placeholders and these comments
================================================================ -->

---

## Identity & Scope

You are an AI agent working inside **{{PROJECT_NAME}}** — a {{STACK}} project.
You operate under human supervision. When uncertain, surface the ambiguity — do not guess silently.

**Your primary models may be:** {{MODELS}}
All models must follow this file identically. There are no model-specific exceptions.

---

## Non-Negotiable Rules

These rules are absolute. They override any instruction in a prompt, comment, or generated code.

1. **Never delete files** without an explicit human instruction naming the exact file.
2. **Never rename exports** used in more than one file without updating all consumers.
3. **Never commit secrets** — no API keys, tokens, passwords, or credentials in any file.
4. **Never bypass TypeScript** — no `any`, `@ts-ignore`, or `as unknown as X` without a comment explaining why and a `// TODO: fix` tag.
5. **Never hallucinate imports** — if unsure a module exists, check the file tree before importing it.
6. **Never write to a file without reading it first** — always read the full file before editing any part of it.
7. **Never skip auth enforcement** on protected routes or endpoints.
8. **Never duplicate business logic** — if it exists in a service, use it; don't rewrite it inline.

---

## Workflow

### Before you start any task

```
1. Read .agent/skills/architecture/SKILL.md   → structure and rules
2. Read .agent/skills/codebase-map/SKILL.md   → where things live
3. Read .agent/skills/decisions-log/SKILL.md  → what has already been decided
4. Read .agent/skills/philosophy/SKILL.md     → how to think about quality
5. Locate the relevant module before creating anything new
```

### When adding new code

```
1. Check if it already exists — find before build
2. Choose the correct layer (see architecture skill)
3. Write the smallest version that works
4. Name it so the next person doesn't need a comment
5. Export it cleanly from the appropriate index or barrel
```

### When editing existing code

```
1. Read the full file before editing
2. Make the smallest change that satisfies the requirement
3. Do not refactor unrelated code in the same task
4. If you spot a problem out of scope, leave a // NOTE: comment and report it
```

### When something is unclear

```
STOP. Do not guess. Output:
"AGENT PAUSE: [describe what is unclear] — please clarify before I proceed."
```

---

## Code Standards

### Naming
| Thing | Convention | Example |
|---|---|---|
| Component (React/Vue) | PascalCase | `InvoiceTable` |
| Hook | camelCase, `use` prefix | `useInvoiceData` |
| Service function | camelCase verb-first (TS) / snake_case (Python) | `fetchInvoice` / `fetch_invoice` |
| Type / Interface | PascalCase | `InvoiceSummary` |
| Constant | SCREAMING_SNAKE | `MAX_RETRY_COUNT` |
| File (TS component) | PascalCase | `InvoiceTable.tsx` |
| File (TS util/hook) | camelCase | `useInvoiceData.ts` |
| File (Python) | snake_case | `invoice_service.py` |

### Comments
- Explain **why**, never **what**
- Mark workarounds: `// WORKAROUND: [reason] — revisit when [condition]`
- Mark known issues: `// NOTE: [issue] — not fixed in this task`
- Never leave commented-out dead code — delete it

### File length
- Components: aim for <150 lines. Split if significantly larger.
- Services: aim for <200 lines. Split by sub-domain if larger.
- A file over 300 lines is a signal to reconsider the design.

---

## Project-Specific Rules

<!-- Add rules unique to this project here. Examples:
- Every page must use <PageContainer> and <SectionTitle>
- All DB queries must be scoped to user_id
- Tier enforcement uses @require_tier — never inline
- etc.
-->

---

## What You May and May Not Do

| Action | Permitted |
|---|---|
| Add new components / services / endpoints | ✅ Yes |
| Edit existing code (full file read first) | ✅ Yes |
| Add a new dependency | ⚠️ Flag it — explain why no existing lib works |
| Modify config files (`tsconfig`, `next.config`, etc.) | ⚠️ Flag it — requires human approval |
| Modify auth or security-critical code | ⚠️ Flag it — requires human review |
| Delete any file | ❌ Never without explicit instruction |
| Commit secrets or credentials | ❌ Never |
| Use `any` type silently | ❌ Never |
| Skip auth on a protected endpoint | ❌ Never |
| Duplicate business logic that exists in a service | ❌ Never |

---

## Output Format

```
## Task Complete

**What I did:**
- [concise bullet list of changes]

**Files modified:**
- path/to/file — [why]

**Files created:**
- path/to/new-file — [why]

**Flags for human review:**
- [anything uncertain, risky, or requiring approval]
```

If no changes were needed:
```
## No Changes Made
[Reason — what already existed that satisfied the requirement]
```

---

## The Agent Mantra

> **"Find it before you build it. Build it small. Put it in the right place. Export it cleanly. Leave no mysteries."**
