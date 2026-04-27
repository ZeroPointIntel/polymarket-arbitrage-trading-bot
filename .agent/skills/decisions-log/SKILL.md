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

<!-- ================================================================
  SETUP INSTRUCTIONS (remove this block when done)
  1. Delete the example decisions below (D-001 through D-003)
  2. Add your own decisions using the template
  3. Keep the template and the "Adding a New Decision" section
  4. Delete these comments
================================================================ -->

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

## Example Decisions
<!-- Replace these with your own. They are illustrative only. -->

## D-001: {{Framework Choice}}

**Status:** Accepted
**Date:** {{YYYY-MM-DD}}

**Decision:** {{e.g. Use Next.js App Router rather than Pages Router.}}

**Context:** {{Why was this decision needed?}}

**Rationale:** {{Why this option? What made it the right choice?}}

**Consequences:**
- {{What does this mean for the codebase going forward?}}
- {{Any trade-offs or ongoing constraints?}}

**Rejected:** {{What alternatives were considered and why they lost.}}

---

## D-002: {{State Management}}

**Status:** Accepted
**Date:** {{YYYY-MM-DD}}

**Decision:** {{e.g. Zustand for global UI state, React Query for server state.}}

**Context:** {{Why was a decision needed here?}}

**Rationale:** {{Why this combination?}}

**Consequences:**
- {{What this means for how state is written.}}

**Rejected:** {{e.g. Redux (too much boilerplate), SWR (weaker mutation handling).}}

---

## D-003: {{Auth Strategy}}

**Status:** Accepted
**Date:** {{YYYY-MM-DD}}

**Decision:** {{e.g. JWT in httpOnly cookies / localStorage / NextAuth.}}

**Context:** {{Why was an auth decision needed?}}

**Rationale:** {{Why this approach?}}

**Consequences:**
- {{Security implications.}}
- {{How tokens are accessed in code.}}

**Rejected:** {{Alternatives considered.}}

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
