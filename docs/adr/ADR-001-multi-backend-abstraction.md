---
doc_id: "DBS-ADR-001"
doc_title: "ADR-001: Multi-Backend Database Abstraction"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Accepted"
project: "database_system"
category: "ADR"
---

# ADR-001: Multi-Backend Database Abstraction

> **SSOT**: This document is the single source of truth for **ADR-001: Multi-Backend Database Abstraction**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2025-01-01 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

The kcenon ecosystem requires persistent storage for various use cases:
- pacs_system: DICOM metadata indexing (relational, high-volume queries)
- monitoring_system: Time-series metrics storage
- Application configuration and state

Different use cases favor different database engines. Coupling application code
to a specific database vendor creates lock-in and prevents deployment flexibility.

Requirements:
1. Support multiple database backends (PostgreSQL, SQLite, MongoDB, Redis).
2. Application code must be backend-agnostic — switching backends should not
   require code changes beyond configuration.
3. Each backend must support the full query API (CRUD, transactions, batch).
4. Adding a new backend must not require modifying existing code.

## Decision

**Implement a Strategy pattern with CRTP base and runtime factory** for
backend-agnostic database access.

Three-layer design:

1. **`database_backend`** (interface) — Pure virtual base defining the database
   contract: `connect()`, `execute()`, `query()`, `begin_transaction()`, etc.
   All methods return `Result<T>`.

2. **`backend_base<Derived, BackendType>`** (CRTP base) — Eliminates ~150 lines
   of boilerplate per backend by providing common implementations (connection
   state management, error mapping, metric hooks).

3. **`backend_registry`** (factory) — Maps backend type enums to factory functions.
   Runtime backend selection via configuration string without `#ifdef`.

```cpp
// Backend-agnostic usage
auto db = unified_database_system::builder()
    .backend("postgresql")          // or "sqlite", "mongodb", "redis"
    .connection_string("host=...")
    .build();

auto result = db->query("SELECT * FROM patients WHERE id = ?", patient_id);
```

## Alternatives Considered

### Direct Backend Usage (No Abstraction)

- **Pros**: Full access to backend-specific features, no abstraction overhead.
- **Cons**: Application code tightly coupled to one database. Switching backends
  requires rewriting all database interaction code. Testing requires a running
  database instance.

### ORM-Only Approach

- **Pros**: High-level, object-oriented database access.
- **Cons**: ORMs struggle with complex queries, bulk operations, and
  backend-specific optimizations. The entity system (`ENTITY_TABLE`,
  `ENTITY_FIELD`) is provided as an optional layer on top of the abstraction,
  not as a replacement.

### Compile-Time Backend Selection (Templates)

- **Pros**: Zero runtime dispatch overhead, type-safe.
- **Cons**: Prevents runtime backend switching (e.g., development on SQLite,
  production on PostgreSQL). Forces all consuming code to be templated.

## Consequences

### Positive

- **Vendor independence**: Applications written against `database_backend` can
  switch between PostgreSQL, SQLite, MongoDB, and Redis via configuration.
- **CRTP efficiency**: `backend_base` eliminates boilerplate while preserving
  compile-time dispatch for internal operations.
- **Extensible**: New backends are added by implementing `database_backend` and
  registering with `backend_registry`. No existing code is modified.
- **Testable**: Mock backends can be registered for unit testing without
  requiring a running database server.

### Negative

- **Lowest common denominator**: The `database_backend` interface exposes only
  operations supported by all backends. Backend-specific features (PostgreSQL
  LISTEN/NOTIFY, Redis pub/sub) require downcasting or extension interfaces.
- **Query portability**: SQL queries must be written in a portable dialect or
  use the `sql_dialect` abstraction. Complex joins and CTEs may not work
  identically across backends.
- **Runtime dispatch**: Virtual function calls add ~2-3 ns per database
  operation. Negligible compared to I/O latency (microseconds to milliseconds).
