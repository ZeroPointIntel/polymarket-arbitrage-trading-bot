---
name: project-frontend
description: >
  Frontend-specific conventions, patterns, and rules for this project. Load this
  skill when working on any UI component, page, hook, styling, state management,
  or client-side logic. Covers component design, styling conventions, state layers,
  accessibility, and performance patterns specific to the frontend stack.
---

# FRONTEND.md
### Frontend Conventions & Patterns

<!-- ================================================================
  SETUP INSTRUCTIONS (remove this block when done)
  1. Replace {{PROJECT_NAME}} with your project name
  2. Fill in the stack-specific sections that apply
  3. Update the Standardised Components section with your actual mandatory components
  4. Remove sections that don't apply to your stack
  5. Delete all {{ }} placeholders and these comments
================================================================ -->

---

## Stack

| Tool | Purpose |
|---|---|
| {{Next.js / React / Vue}} | UI framework |
| {{TypeScript}} | Language |
| {{TailwindCSS / CSS Modules}} | Styling |
| {{Zustand / Jotai / Context}} | Global state |
| {{React Query / SWR}} | Server state |
| {{Framer Motion}} | Animation |
| {{Vitest / Jest}} | Unit testing |
| {{Playwright / Cypress}} | E2E testing |

---

## §1  Component Design

### 1.1  Component Tiers

Every component belongs to exactly one tier. Never skip tiers.

| Tier | Location | Rules |
|---|---|---|
| **Primitive** | `{{src/components/ui/}}` | No business logic. No data fetching. No context reads. |
| **Shared Layout** | `{{src/components/layouts/}}` | Structural shells only. Accepts children via slots/props. |
| **Feature Component** | `{{src/components/}}` | May read context. Must not call APIs directly. |
| **Page Component** | `{{src/app/route/page.tsx}}` | Orchestration only. Fetches data. Passes it down. |

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

export const Button: React.FC<ButtonProps> = ({
  label,
  variant = 'primary',
  isLoading = false,
  isDisabled = false,
  onClick,
}) => { ... };
```

### 1.3  Mandatory Layout Components
<!-- Replace with your actual mandatory components -->

Every content page **must** use these. No exceptions.

| Component | Responsibility | Required on |
|---|---|---|
| `{{PageContainer}}` | Max-width, centring, consistent spacing | Every content page |
| `{{SectionTitle}}` | Unified page header styling | Every page header |

```tsx
// Every page looks like this
export default function MyPage() {
  return (
    <{{PageContainer}}>
      <{{SectionTitle}} title="My Page" />
      {/* content */}
    </{{PageContainer}}>
  );
}
```

---

## §2  Styling

### 2.1  Design Tokens
<!-- Update the token location for your project -->

All visual values come from design tokens defined in `{{src/app/globals.css}}`.
Never hardcode colours, spacing, or font sizes directly in components.

```css
/* ✅ Use tokens */
color: var(--color-primary);
padding: var(--spacing-4);

/* ❌ Never hardcode */
color: #1E3A5F;
padding: 16px;
```

### 2.2  Styling Rules

- One stylesheet per component (`{{ComponentName.module.css}}` or Tailwind utilities).
- No global class names that might collide.
- Dark mode must be handled — every colour choice must work in both modes.
- No inline `style` objects for anything that could be a token or utility class.

---

## §3  State Management

### 3.1  Which State Goes Where

```
Is it server data?  → React Query / SWR hook
Is it shared UI state (auth, theme)?  → Context / Zustand store
Is it local to one component?  → useState / useReducer
```

Never store server data in a Zustand store. Never fetch in a component body.

### 3.2  Data Fetching Pattern

Data fetching lives in hooks, not in component bodies.

```ts
// ✅ Hook owns the fetching
export function useOrders() {
  return useQuery({
    queryKey: ['orders'],
    queryFn: () => fetchOrders(token),
  });
}

// ✅ Component consumes the hook
function OrdersPage() {
  const { data, isLoading, error } = useOrders();
  ...
}

// ❌ Never fetch in a component body
function OrdersPage() {
  const [orders, setOrders] = useState([]);
  useEffect(() => { fetch('/api/orders').then(...) }, []);
}
```

### 3.3  Context Rules

Context is for **global** state that many components need. It is not a state management replacement.

- Keep providers thin — delegate logic to hooks they expose.
- A context that does data fetching is a context that's doing too much.
- Name contexts clearly: `AuthContext`, `ThemeContext`, `SubscriptionContext`.

---

## §4  Error Handling

### 4.1  Never Silently Swallow Errors

```ts
// ❌ Silent failure — user sees nothing, developer sees nothing
try {
  await saveOrder(order);
} catch (e) {}

// ✅ Explicit failure — user informed, error logged
try {
  await saveOrder(order);
} catch (e) {
  toast.error('Failed to save order. Please try again.');
  console.error('[saveOrder]', e);
}
```

### 4.2  Error States Are UI States

Loading, error, and empty are first-class states. Every component that fetches data must handle all three.

```tsx
if (isLoading) return <LoadingSpinner />;
if (error) return <ErrorState message={error.message} />;
if (!data.length) return <EmptyState message="No orders yet" />;
return <OrderList orders={data} />;
```

---

## §5  Accessibility

Non-negotiable minimums for every interactive component:

- All `<button>` and `<a>` elements have visible focus styles.
- All images have meaningful `alt` text (or `alt=""` for decorative images).
- All form inputs have associated `<label>` elements.
- All icon-only buttons have an `aria-label`.
- Colour is never the only way information is conveyed.
- All interactive components are operable by keyboard alone.

---

## §6  Performance

### 6.1  Defaults

- Images use `next/image` (or equivalent) — never raw `<img>` tags.
- Heavy components are lazy-loaded with `dynamic(() => import(...))`.
- Lists with many items use virtualisation — never render 1000+ DOM nodes.
- `useEffect` dependencies are explicit and minimal — no `[]` on effects with dependencies.

### 6.2  What Not to Optimise Prematurely

Do not reach for `useMemo`, `useCallback`, or `React.memo` until a performance problem is measured. Premature memoisation adds complexity and often makes performance worse. Profile first, optimise second.

---

## §7  Agent Reminders

Before submitting any frontend change:

- [ ] Does the new page use `{{PageContainer}}` and `{{SectionTitle}}`?
- [ ] Does the component use design tokens — no hardcoded colours or spacing?
- [ ] Are loading, error, and empty states all handled?
- [ ] Is the component keyboard-accessible?
- [ ] Is data fetching in a hook — not in the component body?
- [ ] Is the component under ~150 lines? If not, does it need splitting?
