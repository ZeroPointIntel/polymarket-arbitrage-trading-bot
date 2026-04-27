---
name: project-philosophy
description: >
  The engineering philosophy of this codebase. Load this skill before designing
  any new feature, writing a new function or component, refactoring existing code,
  or making any structural decision. Also load when asked to review code quality,
  assess a design, or choose between two approaches. This skill defines what "good"
  looks like — not just what is correct, but what is excellent.
---

# PHILOSOPHY.md
### Engineering Philosophy
> How we think about code quality, elegance, and long-term maintainability.
> This is not a list of rules. It is a way of seeing.

---

## The Three Commitments

Every line of code makes an implicit promise to the next person who reads it — human or agent. We make three commitments:

1. **Simplicity** — the solution is as small as the problem demands. No more.
2. **Elegance** — the code reads like well-written prose. The intent is obvious.
3. **Deletability** — every piece can be understood, changed, or removed without fear.

These are not in tension. A simple solution is usually elegant. An elegant solution is usually easy to delete. When you find yourself sacrificing one, you are probably solving the wrong problem.

---

## §1  Simplicity

> The best code is code that doesn't need to exist.

### 1.1  The Boring Solution Wins

Before writing anything, ask: is there a boring way to do this? A plain `fetch` call. A single function. A straightforward `if` statement.

Boring solutions are easy to read during an incident, easy for a new agent to understand without context, and easy to replace when requirements change. Clever solutions are a debt you pay forever.

**Default to boring.**

### 1.2  Delete Before You Add

When a requirement changes, your first instinct should be to **remove** code, not add more. New requirements often mean old code is wrong — not that new code is needed on top of it.

Before adding a new prop, ask: should this component be split instead?
Before adding a new parameter, ask: should this be two functions instead?
Before adding a new endpoint, ask: does this belong in an existing one?

### 1.3  The Five-Line Rule

If a function is doing something that takes more than ~5 lines to explain in plain English, it is probably doing two things. Name both things. Write two functions.

```ts
// ❌ One function, two jobs
async function processAndSaveOrder(orderId: string, userId: string) {
  const order = await db.getOrder(orderId, userId);
  const total = order.items.reduce((s, i) => s + i.price * i.qty, 0);
  const withTax = { ...order, total: total * 1.2 };
  await db.saveOrder(withTax);
  await emailService.sendConfirmation(userId, withTax);
  return withTax;
}

// ✅ Each function has one job
function calculateOrderTotal(order: Order): number {
  const subtotal = order.items.reduce((s, i) => s + i.price * i.qty, 0);
  return subtotal * (1 + order.taxRate);
}

async function finaliseOrder(orderId: string, userId: string): Promise<Order> {
  const order = await db.getOrder(orderId, userId);
  const finalised = { ...order, total: calculateOrderTotal(order) };
  await db.saveOrder(finalised);
  await emailService.sendConfirmation(userId, finalised);
  return finalised;
}
```

### 1.4  No Speculative Abstraction

Do not build infrastructure for problems you don't have yet. No "we might need this later." No base classes for a hierarchy of one. No plugin systems for a single use case.

> Build for today's problem with tomorrow's clarity in mind — not tomorrow's imagined problem with today's code.

### 1.5  You Aren't Gonna Need It (YAGNI)

The most maintainable code is code that was never written. Every line you add is a line that must be read, understood, tested, and eventually deleted. Add only what the current requirement demands.

---

## §2  Elegance

> Elegant code has a shape. You can see what it does before you read it.

### 2.1  Names Are the Primary Documentation

If a function or variable needs a comment to explain what it is, the name is wrong. Rename first. Comment only to explain *why* something surprising is true — never *what* the code does.

```ts
// ❌ Comment explaining what the code does
// Check if user has an active paid subscription
const s = user.sub;
if (s === 'pro' || s === 'ai') { ... }

// ✅ The name explains everything
const userHasPaidAccess = ['pro', 'ai'].includes(user.subscriptionTier);
if (userHasPaidAccess) { ... }
```

### 2.2  Functions Should Read Like Sentences

A well-named function with well-named arguments reads like a sentence. When you read a call site, you should understand it without opening the implementation.

```ts
// ❌ Requires reading the implementation
render(data, true, false, 'xl')

// ✅ Reads like a sentence
renderPageHeader({ title: 'Dashboard', showBackButton: false, size: 'xl' })
```

### 2.3  Flat Is Better Than Nested

Every level of nesting is a cognitive tax. Invert conditions and return early. A function with three levels of nesting is three functions pretending to be one.

```ts
// ❌ Pyramid of doom
function handleRequest(user, resource) {
  if (user) {
    if (user.isActive) {
      if (user.canAccess(resource)) {
        return processRequest(user, resource);
      } else {
        return forbidden();
      }
    } else {
      return inactive();
    }
  } else {
    return unauthorized();
  }
}

// ✅ Flat — failures exit early, happy path is obvious
function handleRequest(user, resource) {
  if (!user) return unauthorized();
  if (!user.isActive) return inactive();
  if (!user.canAccess(resource)) return forbidden();
  return processRequest(user, resource);
}
```

### 2.4  Symmetry Is a Signal

When two things are conceptually the same, they should look the same in code. When two things are conceptually different, they should look different. Inconsistency in form signals inconsistency in thinking.

If one page uses `<PageContainer>` and another uses a raw `<div className="max-w-7xl ...">`, a reader has to wonder: are these different on purpose, or by accident? Always the same concept, always the same form.

### 2.5  One Level of Abstraction Per Function

A function should operate at one level of abstraction. Don't mix high-level orchestration with low-level detail in the same body. A function that calls `fetchOrders()` should not also be manually parsing response headers.

---

## §3  Deletability

> The measure of good architecture is not how easy it is to add things. It is how easy it is to remove them.

### 3.1  Write for the Next Agent

Every function, component, and module you write will be read by someone with no context. That someone may be a future AI agent, a new developer, or you in six months.

Ask yourself: if someone deleted this file and found it in a code review, would they understand immediately what it does, why it exists, and how to use it?

If not — simplify it until the answer is yes.

### 3.2  Avoid Hidden Coupling

Coupling is not just about imports. It is about **assumptions**. Code is coupled when changing one thing requires understanding another.

Signs of hidden coupling:
- A component that only works if a specific context is mounted above it (but doesn't say so)
- A service that assumes a specific DB column name without using the model layer
- A function that behaves differently depending on a global variable
- Logic that is duplicated "because it's slightly different" — it will diverge and break

Make dependencies **explicit and visible**. Pass them in. Name them clearly.

### 3.3  Prefer Composition Over Configuration

A component with 12 props is hard to delete because every consumer depends on a different combination. A component with 3 props and clear children slots is easy to extend, easy to change, and easy to remove.

```tsx
// ❌ Configuration-heavy — hard to change anything without checking every consumer
<DataTable data={rows} columns={cols} sortable paginated pageSize={20}
  onRowClick={fn} emptyState="None" loading={l} error={e} headerStyle="dark" />

// ✅ Composed — each concern is isolated
<DataTable data={rows} columns={cols} onRowClick={fn}>
  <DataTable.LoadingState visible={isLoading} />
  <DataTable.EmptyState message="No data" />
  <DataTable.Pagination pageSize={20} />
</DataTable>
```

### 3.4  Leave the Campsite Cleaner

Every time you touch a file, leave it slightly better than you found it. Not a full refactor — just one small improvement. A better variable name. A removed comment that describes what the code obviously does. A function extracted from a 40-line block.

Over time, this compounds. A codebase maintained this way improves continuously without ever requiring a dedicated cleanup sprint.

### 3.5  If It's Hard to Test, the Design Is Wrong

Untestable code is tightly coupled code. If you can't write a test for a function without standing up the entire application, that function knows too much about its environment.

Pure functions are trivially testable. Services that take typed inputs and return typed outputs are trivially testable. Components that receive props and render deterministically are trivially testable.

When something is hard to test, that is the design telling you to refactor it — not a reason to skip the test.

---

## §4  For AI Agents Specifically

### 4.1  You Tend Toward Local Optima

You are very good at solving the immediate problem. You are less good at noticing when solving the immediate problem creates a worse global state. Before finishing any task, zoom out:

- Does this solution make the next task harder?
- Does this add a concept that didn't exist before, and was that necessary?
- Could someone delete what I just wrote in six months without regret?

### 4.2  The Over-Engineering Traps

Watch for these patterns in your own output — they are the most common agent failure modes:

| Trap | What it looks like | Better approach |
|---|---|---|
| Premature abstraction | A base class or HOC for a single use case | Write it inline; abstract on the third occurrence |
| Config explosion | A component with 8+ props to handle edge cases | Split into two simpler components |
| Defensive nesting | Five levels of try/catch wrapping everything | Fail fast, handle errors at the boundary |
| Phantom requirements | Building for scale/flexibility not yet needed | Solve the stated problem only |
| Comment overload | Explaining what every line does | Rename things until the comments are unnecessary |

### 4.3  The Three Questions Before You Ship

Before completing any task, read your output once as if you are seeing it for the first time. Ask:

1. **Is this simple?** Could it be shorter without losing clarity?
2. **Is this honest?** Does it do exactly what it says — nothing more, nothing hidden?
3. **Is this kind?** Will the next person — human or agent — be grateful you wrote it this way?

If the answer to all three is yes, ship it.

---

## The Final Word

> Good code is not code that works. Code that works is the baseline.
> Good code is code that the next person can understand, trust, change, and — when the time comes — delete without fear.

> *"The purpose of abstraction is not to be vague, but to create a new semantic level in which one can be absolutely precise."*
> — Edsger W. Dijkstra
