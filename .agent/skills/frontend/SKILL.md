---
name: project-frontend
description: >
  Frontend-specific conventions, patterns, and rules for this project. Load this
  skill when working on any Next.js UI component, page, hook, styling, state management,
  or client-side logic. Covers component design, styling conventions, state layers,
  accessibility, and performance patterns specific to the frontend stack.
---

# FRONTEND.md
### Frontend Conventions & Patterns

---

## Stack

| Tool | Purpose |
|---|---|
| Next.js 14+ (App Router) | UI framework |
| TypeScript | Language |
| TailwindCSS + shadcn/ui | Styling |
| Zustand | Global state |
| TanStack Query | Server state |
| Server-Sent Events (SSE) | Real-time state |

---

## §1  Component Design

### 1.1  Component Tiers

Every component belongs to exactly one tier. Never skip tiers.

| Tier | Location | Rules |
|---|---|---|
| **Primitive** | `frontend/src/components/ui/` | No business logic. No data fetching. No context reads. |
| **Shared Layout** | `frontend/src/components/layouts/` | Structural shells only. Accepts children via slots/props. |
| **Feature Component** | `frontend/src/components/` | May read Zustand/TanStack. Must not call APIs directly. |
| **Page Component** | `frontend/src/app/route/page.tsx` | RSC (React Server Components). Fetches data. Passes it down. |

### 1.2  Component Contract

Every component declares its contract at the top. Props flow down, events flow up.

```tsx
// ✅ Explicit, typed, documented
interface ButtonProps {
  readonly label: string;
  readonly variant?: 'primary' | 'secondary' | 'ghost';
  readonly isLoading?: boolean;
  readonly isDisabled?: boolean;
  readonly onClick?: () => void;  // no event object leaked to consumer
}
```

---

## §2  State Management

### 2.1  Which State Goes Where

```
Is it historical trade data?  → TanStack Query
Is it live PnL/Edge?  → SSE Hook / Zustand Store
Is it a user preference?  → Zustand Store
Is it local to one component?  → useState
```

Never fetch in a component body. Use dedicated hooks or RSCs.

---

## §3  Auth & Security

### 3.1  WebAuthn Challenge Rules

Any action that modifies the bot's state requires a fresh WebAuthn challenge.

```ts
// ✅ Hook owns the auth flow
export function useKillSwitch() {
  const { triggerAuth } = useWebAuthn();
  
  return useMutation({
    mutationFn: async () => {
      const challenge = await triggerAuth();
      return fetch('/api/kill-switch', {
        method: 'POST',
        body: JSON.stringify({ challenge })
      });
    }
  });
}
```

---

## §4  Performance & Data Flow

### 4.1  Real-Time Updates via SSE

The frontend does not connect to the C++ core or Redis directly. It subscribes to a Server-Sent Events (SSE) stream provided by the API Gateway.

```ts
// ✅ SSE for live updates
export function useLiveState() {
  const [state, setState] = useState<LiveState>(defaultState);
  
  useEffect(() => {
    const eventSource = new EventSource('/api/live');
    eventSource.onmessage = (event) => setState(JSON.parse(event.data));
    return () => eventSource.close();
  }, []);
  
  return state;
}
```

---

## §5  Agent Reminders

Before submitting any frontend change:

- [ ] Does the new feature component use TanStack Query or an SSE hook instead of `fetch` inside `useEffect`?
- [ ] Do state-changing intents trigger a WebAuthn challenge?
- [ ] Is the component using Tailwind/shadcn tokens instead of hardcoded styles?
- [ ] Are loading, error, and empty states all handled?
