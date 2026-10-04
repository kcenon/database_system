---
doc_id: "DBS-POOL-001"
doc_title: "Connection Pool"
doc_version: "1.0.0"
doc_date: "2026-04-18"
doc_status: "Active"
project: "database_system"
category: "architecture"
---

# Connection Pool

> **SSOT**: This document is the single source of truth for the connection pool
> introduced under issue [#568](https://github.com/kcenon/database_system/issues/568).

database_system provides an in-process, thread-safe connection pool for reusing
`database_backend` instances across concurrent callers. The pool is **additive**
and does not alter any existing direct-connection API. Users who do not need
pooling can continue using `database_backend::initialize()` / `shutdown()` directly.

## When to Use the Pool

| Scenario | Recommendation |
|----------|---------------|
| Single-threaded script, short-lived connection | Direct backend (no pool) |
| Server handling concurrent requests on PostgreSQL | Pool (PG connections are not thread-safe) |
| High-QPS workload paying the handshake per query | Pool with `min_size > 0` |
| SQLite embedded, single process | Pool-of-one or direct backend |

## Public API

```cpp
#include <kcenon/database/core/connection_pool.h>
#include <kcenon/database/core/backend_registry.h>

using namespace database::core;

pool::pool_config cfg;
cfg.min_size = 2;
cfg.max_size = 16;
cfg.acquire_timeout = std::chrono::seconds(5);
cfg.validate_on_acquire = true;

auto pool = pool::connection_pool::create(
    cfg,
    /* factory */ [config]() -> std::unique_ptr<database_backend> {
        auto backend = backend_registry::instance().create("postgresql");
        if (!backend) return nullptr;
        auto init = backend->initialize(config);
        if (init.is_err()) return nullptr;
        return backend;
    });

// Acquire a lease. RAII returns the connection on scope exit.
{
    auto lease = pool->acquire();
    if (lease.is_err()) {
        // Pool exhausted, shut down, or factory failed.
        return;
    }
    auto rows = lease.value()->select_query("SELECT 1");
    // lease is returned to the pool here
}
```

## Guarantees

- **Thread safety** — `acquire()`, `stats()`, `shutdown()` are safe to call from
  any thread. Each issued `pooled_connection` is owned by exactly one thread.
- **RAII return** — A connection is always returned to the pool when the lease
  handle is destroyed, even on exception.
- **Bounded growth** — `total_connections <= max_size` at all times.
- **Back-pressure** — When the pool is saturated, `acquire()` blocks up to
  `acquire_timeout` and returns `error_code::timeout` on expiry.
- **Broken-connection recovery** — Callers may call `lease.mark_broken()` to
  signal the connection is no longer usable; the pool discards it on return
  and creates a replacement on the next acquire. If `validate_on_acquire` is
  enabled (default), connections failing the validator are transparently
  replaced.
- **No lost increments** — Statistics (`total_acquires`, `acquire_timeouts`,
  `failed_creations`, `replaced_connections`) are updated under the pool mutex.

## Design Notes

- The pool holds `std::unique_ptr<database_backend>` and hands out move-only
  `pooled_connection` lease objects. The lease forwards `operator->` to the
  wrapped backend.
- Idle connections are kept in a LIFO queue so the most-recently-used
  connection is handed out first (better cache warmth).
- The factory callback may block on network I/O; the pool releases its mutex
  around factory invocations to keep other acquires unblocked.
- The default validator calls `backend.is_initialized()`. A driver-aware
  validator (e.g., `"SELECT 1"`) can be supplied as the third argument to
  `create()`.

## Limitations

- The pool does not currently integrate automatically into
  `unified_database_system`. Callers who want the builder-style API
  (`.set_pool_size(...)`) must wire the pool in explicitly. Integration is
  tracked as a follow-up.
- No background idle-timeout eviction yet. Idle connections persist until
  `shutdown()` or the owning `shared_ptr<connection_pool>` is released.

## Relationship to Historical ADR-002

ADR-002 (Superseded) described a previous local pool design that was removed
in Phase 4.3 in anticipation of a server-side `ProxyMode`. `ProxyMode` remains
unimplemented. This pool re-introduces the local pooling capability with a
smaller, more focused surface area (backend-agnostic, header + single .cpp).

## References

- Issue [#568](https://github.com/kcenon/database_system/issues/568) —
  "implement ProxyMode or re-introduce connection pool"
- Issue [#573](https://github.com/kcenon/database_system/issues/573) —
  Thread-safe PG connection handling; addressed via this pool.
- Header: `include/kcenon/database/core/connection_pool.h`
- Tests: `tests/connection_pool_test.cpp`
