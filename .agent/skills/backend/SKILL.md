---
name: project-backend
description: >
  Backend-specific conventions, patterns, and rules for this project. Load this
  skill when working on any API endpoint, service function, database query, auth
  logic, data model, or server-side business rule. Covers handler design, service
  patterns, data access, error handling, auth enforcement, and testing conventions
  specific to the backend stack.
---

# BACKEND.md
### Backend Conventions & Patterns

<!-- ================================================================
  SETUP INSTRUCTIONS (remove this block when done)
  1. Replace {{PROJECT_NAME}} with your project name
  2. Fill in the stack table with your actual technologies
  3. Keep only the language sections that apply (Python / TypeScript / both)
  4. Update the auth pattern for your actual auth system
  5. Update the DB section for your actual database
  6. Delete all {{ }} placeholders and these comments
================================================================ -->

---

## Stack

| Tool | Purpose |
|---|---|
| {{FastAPI / Express / Hono}} | HTTP framework |
| {{Python / TypeScript}} | Language |
| {{PostgreSQL / SQLite / MongoDB}} | Database |
| {{Pydantic / Zod}} | Schema validation |
| {{SQLAlchemy / Prisma / Drizzle}} | ORM / query builder |
| {{JWT / Sessions / Clerk}} | Authentication |
| {{Pytest / Vitest}} | Testing |

---

## §1  Handler Design

### 1.1  The Handler Contract

Route handlers do exactly three things. No more.

```python
# Python / FastAPI
@router.post("/orders")
async def create_order(
    body: OrderCreate,              # 1. Input validated by Pydantic
    user = Depends(get_current_user) # 2. Auth enforced by dependency
):
    return await order_service.create(body, user.id)  # 3. Delegate to service
```

```ts
// TypeScript / Express or Hono
router.post('/orders', authenticate, async (req, res) => {
  const body = OrderCreateSchema.parse(req.body);     // 1. Validate
  // auth already enforced by middleware               // 2. Auth
  const result = await orderService.create(body, req.user.id); // 3. Delegate
  res.json(result);
});
```

A handler that contains business logic is a handler that needs refactoring.

### 1.2  Auth Enforcement

**Every protected endpoint enforces auth. No exceptions.**

```python
# Python — FastAPI dependency injection
@router.get("/protected")
async def protected_route(user = Depends(get_current_user)):
    ...

# ❌ This is never acceptable
@router.get("/protected")
async def protected_route():
    # No auth check — this is a security hole
    ...
```

```ts
// TypeScript — middleware
router.get('/protected', authenticate, async (req, res) => { ... });
```

### 1.3  User Data Isolation

All queries for user-owned data are scoped by `user_id`. Never return all rows.

```python
# ✅ Scoped — only returns this user's data
orders = await db.orders.find_all(where={"user_id": user.id})

# ❌ Returns everyone's data — security hole
orders = await db.orders.find_all()
```

---

## §2  Service Design

### 2.1  Services Are the Business Logic Layer

Services contain all business rules. They are independent of HTTP — no `Request`, `Response`, or framework objects inside a service.

```python
# ✅ Pure service — no HTTP concerns, fully testable
class OrderService:
    async def create(self, data: OrderCreate, user_id: str) -> Order:
        total = self._calculate_total(data.items, data.tax_rate)
        order = await self.db.orders.insert({
            **data.dict(),
            "total": total,
            "user_id": user_id,
        })
        await self.email.send_confirmation(user_id, order)
        return order

    def _calculate_total(self, items: list[LineItem], tax_rate: float) -> float:
        subtotal = sum(i.price * i.qty for i in items)
        return subtotal * (1 + tax_rate)
```

### 2.2  Pure Functions First

Extract pure computation out of side-effecting functions. Pure functions are trivially testable.

```python
# ✅ Pure — test without DB, without email, without anything
def calculate_order_total(items: list[LineItem], tax_rate: float) -> float:
    subtotal = sum(i.price * i.qty for i in items)
    return subtotal * (1 + tax_rate)

# Side effects isolated here
async def create_order(data: OrderCreate, user_id: str) -> Order:
    total = calculate_order_total(data.items, data.tax_rate)  # pure call
    return await db.orders.insert({...})                       # side effect
```

### 2.3  Typed Inputs and Outputs

Every service function has typed inputs and a typed return. No `dict`, no `Any`, no untyped returns.

```python
# ✅ Fully typed
async def get_user_portfolio(user_id: str) -> PortfolioSummary: ...

# ❌ Untyped — agents and developers cannot reason about this
async def get_portfolio(user_id) -> dict: ...
```

---

## §3  Data Models

### 3.1  Models Are Data Shapes

Models (Pydantic, Zod, Prisma schema) define data shapes only. No business logic inside models.

```python
# ✅ Shape only
class OrderCreate(BaseModel):
    items: list[LineItem]
    tax_rate: float = 0.2
    shipping_address: Address

# ❌ Business logic in a model
class OrderCreate(BaseModel):
    items: list[LineItem]

    def get_total(self) -> float:
        return sum(i.price * i.qty for i in self.items) * 1.2  # belongs in service
```

### 3.2  Separate Request, Response, and Domain Models

```python
OrderCreate   → what the client sends (input validation)
OrderResponse → what the API returns (never expose internal fields)
Order         → the internal domain object (may have fields not exposed)
```

Never return a raw DB row to the client. Always map through a response model.

---

## §4  Database

### 4.1  The Schema Is the Source of Truth

The database schema file (`{{schema.sql / schema.prisma}}`) is always committed. The database file itself (`{{.db}}`) is never committed.

### 4.2  Never Raw SQL Inline

SQL strings inline in route handlers or services are hard to audit and impossible to type-check. Use the ORM or a dedicated query helper.

```python
# ❌ Raw SQL inline in a service
result = await db.execute("SELECT * FROM orders WHERE user_id = ?", [user_id])

# ✅ Through the model layer
result = await db.orders.find_all(where={"user_id": user_id})
```

### 4.3  Migrations Are Sequential and Immutable

Never edit an existing migration. New changes get a new migration file. Migration history is a permanent audit trail.

---

## §5  Error Handling

### 5.1  Return Errors, Don't Throw Through Boundaries

```python
# ✅ Explicit typed error responses
@router.get("/orders/{id}")
async def get_order(id: str, user = Depends(get_current_user)):
    order = await order_service.get(id, user.id)
    if not order:
        raise HTTPException(status_code=404, detail="Order not found")
    return order
```

### 5.2  Error Response Shape

All error responses follow a consistent shape:

```json
{
  "error": "ORDER_NOT_FOUND",
  "message": "The requested order does not exist or you do not have access.",
  "status": 404
}
```

Never expose internal error messages, stack traces, or DB errors to the client.

### 5.3  Log Errors Server-Side

Every caught exception is logged with context before returning a safe response to the client.

```python
except Exception as e:
    logger.error(f"[order_service.get] user={user_id} order={order_id} error={e}")
    raise HTTPException(status_code=500, detail="Internal server error")
```

---

## §6  Auth Patterns

### 6.1  The Auth Dependency

```python
# Every protected route uses this dependency — never roll your own auth check inline
user = Depends(get_current_user)
```

### 6.2  Tier / Permission Enforcement
<!-- Update for your actual permission system -->

```python
# Tier-gated routes use the guard decorator — never inline tier checks
@router.get("/premium-feature")
@require_tier('pro')
async def premium_feature(user = Depends(get_current_user)):
    ...
```

### 6.3  What the Auth Dependency Must Never Do

The `get_current_user` dependency extracts identity only. It does not enforce permissions or tiers — that belongs in route-level decorators or explicit checks.

---

## §7  Testing

### 7.1  Test Pure Functions First

Pure service functions (no DB, no HTTP) should have unit tests. These run instantly and require no setup.

```python
def test_calculate_order_total():
    items = [LineItem(price=10.0, qty=2), LineItem(price=5.0, qty=1)]
    assert calculate_order_total(items, tax_rate=0.2) == 30.0
```

### 7.2  Integration Tests for Handlers

Handler tests use a test client and a test database. They verify the full path: auth → validation → service → response.

```python
def test_create_order_requires_auth(client):
    response = client.post("/orders", json={...})
    assert response.status_code == 401
```

---

## §8  Agent Reminders

Before submitting any backend change:

- [ ] Does every new protected endpoint use `Depends(get_current_user)` or equivalent?
- [ ] Are all user-data queries scoped by `user_id`?
- [ ] Is business logic in a service — not in the handler?
- [ ] Are all inputs validated by a Pydantic/Zod model?
- [ ] Is the response model a typed schema — not a raw dict or DB row?
- [ ] Are errors logged server-side before returning a safe response?
- [ ] If tier-gated, is enforcement at the route level — not inline?
- [ ] Is there a pure function that can be unit tested?
