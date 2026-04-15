---
doc_id: "DBS-ADR-002"
doc_title: "ADR-002: Connection Pool Architecture"
doc_version: "1.1.0"
doc_date: "2026-04-15"
doc_status: "Superseded"
project: "database_system"
category: "ADR"
---

# ADR-002: Connection Pool Architecture

> **SUPERSEDED (Phase 4.3, 2025-12-09)**: The local connection pool described in this ADR has been **removed**. Production pooling is now handled server-side via ProxyMode / `database_server` middleware. This ADR is retained for historical context only. See [CHANGELOG](../CHANGELOG.md) and the discussion in [README](../../README.md#overview). <!-- TODO: ADR-003 superseding this decision -->

> **SSOT**: This document is the single source of truth for **ADR-002: Connection Pool Architecture** (historical).

| Field | Value |
|-------|-------|
| Status | Superseded (Phase 4.3, 2025-12-09) |
| Date | 2025-04-01 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

Database connections are expensive to establish (TCP handshake, authentication,
SSL negotiation). Creating a new connection per query is unacceptable for
high-throughput applications like pacs_system, which processes concurrent DICOM
operations.

database_system needs connection management that:
1. Reuses connections across queries to amortize setup cost.
2. Supports concurrent access from multiple threads safely.
3. Provides backpressure when all connections are in use.
4. Handles stale connections gracefully (network timeout, server restart).

## Decision

**Implement a thread-safe connection pool with RAII-based connection leasing**
integrated into the `unified_database_system`.

Design:
1. **Pool management** — Pre-allocates a configurable number of connections
   (`min_connections`) and grows up to `max_connections` on demand.
2. **RAII leasing** — `pool_connection` guard object that returns the connection
   to the pool on destruction, preventing connection leaks.
3. **Health checking** — Periodic validation of idle connections; stale
   connections are replaced transparently.
4. **Backpressure** — When all connections are in use, callers block with a
   configurable timeout, returning an error `Result` on timeout.

```cpp
auto db = unified_database_system::builder()
    .backend("postgresql")
    .min_connections(4)
    .max_connections(16)
    .idle_timeout(std::chrono::minutes(5))
    .build();

// Connection automatically leased from pool, returned on scope exit
auto result = db->query("SELECT ...");
```

## Alternatives Considered

### Single Shared Connection with Mutex

- **Pros**: Simplest implementation, no pool management.
- **Cons**: Serializes all database access through one connection. Throughput
  bottleneck for concurrent applications.

### Connection-Per-Thread (Thread-Local)

- **Pros**: No contention, thread-safe by design.
- **Cons**: Connection count tied to thread count. Thread pools with many
  workers would create excessive connections. No sharing between threads
  with different activity levels.

### External Pool Library (pgBouncer, etc.)

- **Pros**: Battle-tested, supports advanced features (transaction pooling,
  connection routing).
- **Cons**: External process dependency complicates deployment. Only works
  for PostgreSQL. The ecosystem needs a backend-agnostic solution.

## Consequences

### Positive

- **Connection reuse**: Amortizes connection setup cost across queries.
  Measured 10-50x throughput improvement for short queries.
- **RAII safety**: Connection leaking is impossible with the `pool_connection`
  guard. Connections are always returned, even on exception.
- **Adaptive sizing**: Pool grows from `min_connections` to `max_connections`
  based on demand, balancing resource usage and throughput.
- **Backend-agnostic**: The pool works with all backends (PostgreSQL, SQLite,
  MongoDB, Redis) through the `database_backend` interface.

### Negative

- **Memory overhead**: Idle connections consume server-side resources. Mitigated
  by `idle_timeout` which closes connections unused for the configured duration.
- **Complexity**: Pool management (growth, shrinkage, health checking, timeout)
  adds significant complexity over simple connection management.
- **Configuration burden**: Users must tune `min_connections`, `max_connections`,
  and `idle_timeout` for their workload. Defaults work for moderate loads but
  may need adjustment for high-throughput scenarios.
