# database_system

## Overview

Modern C++20 database abstraction layer (DAL) providing unified access to multiple
database backends. Eliminates vendor lock-in through a type-safe, backend-agnostic
interface with zero-configuration setup and RAII-based resource management.

## Architecture

```
database/
  core/           - Backend interface, CRTP base, backend_registry, concepts
  backends/       - postgresql, sqlite, mongodb, redis implementations
  query/          - immutable_query_builder (thread-safe, functional style)
  query_builder/  - condition_builder, join_builder, sql_dialect, value_formatter
  orm/            - entity.h (ENTITY_TABLE, ENTITY_FIELD macros)
  integrated/     - unified_database_system (zero-config entry point), adapters
  async/          - async_operations
  monitoring/     - performance_monitor, pool_metrics
  security/       - secure_connection (TLS/SSL)
  protocol/       - database_protocol, serialization
```

Key abstractions:
- `database_backend` — Pure virtual interface (Strategy pattern); all backends implement it
- `backend_base<Derived, Type>` — CRTP base eliminating ~150 lines of boilerplate per backend
- `backend_registry` — Factory pattern for runtime backend selection (no `#ifdef`)
- `unified_database_system` — Zero-config entry point with builder pattern
- `immutable_query_builder` — Thread-safe query construction (functional/immutable style)
- `entity_base` with `ENTITY_TABLE()` / `ENTITY_FIELD()` macros — C++20 concepts-based ORM

## Build & Test

```bash
# Using build scripts
./scripts/dependency.sh   # Install ecosystem deps
./scripts/build.sh        # Build

# Manual CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_POSTGRESQL=ON
cmake --build build
cd build && ctest --output-on-failure
```

Key CMake options:
- `USE_POSTGRESQL` (ON), `USE_SQLITE` (OFF), `USE_MONGODB` (OFF), `USE_REDIS` (OFF)
- `USE_UNIT_TEST` (ON) — Google Test + Google Mock
- `DATABASE_BUILD_BENCHMARKS` (OFF) — Google Benchmark suite
- `DATABASE_BUILD_INTEGRATION_TESTS` (ON)
- `USE_THREAD_SYSTEM` (ON), `USE_MONITORING_SYSTEM` (ON), `USE_CONTAINER_SYSTEM` (ON)

Presets: `default`, `debug`, `release`, `full` (all backends), `asan`, `tsan`, `ubsan`, `ci`, `vcpkg`

CI: Multi-platform (Ubuntu GCC/Clang, macOS, Windows MSVC), coverage, static analysis,
sanitizers, benchmarks, integration tests, CVE scan, SBOM.

## Key Patterns

- **Multi-backend abstraction** — `database_backend` interface + `backend_registry` factory +
  CRTP `backend_base<>`. Four backends: postgresql, sqlite, mongodb, redis
- **ORM macros** — `ENTITY_TABLE()`, `ENTITY_FIELD()`, `ENTITY_METADATA()` with C++20 concepts;
  supports primary_key, auto_increment, not_null, unique, default_value constraints
- **Query builders** — Legacy mutable `query_builder` and newer `immutable_query_builder`
  (thread-safe); both support SQL and NoSQL
- **Result type** — All APIs return `kcenon::common::Result<T>` or `VoidResult`
- **C++20 concepts** — Extensive: `QueryCallback`, `ErrorHandler`, `ConnectionFactory`, etc.
- **Adapter pattern** — Backend-agnostic adapters for logging, monitoring, threading with fallbacks

## Ecosystem Position

**Tier 3** — Mid-level in the ecosystem hierarchy.

```
common_system      (Tier 0) [required]
thread_system      (Tier 1) [optional]
container_system   (Tier 1) [optional]
monitoring_system  (Tier 2) [optional]
database_system    (Tier 3)  <-- this project
```

Consumed by pacs_system (Tier 5).

## Dependencies

**Required**: kcenon-common-system, asio >= 1.30.2, C++20, CMake 3.20+
**Optional ecosystem**: thread_system, logger_system, container_system, monitoring_system
**Backend-specific (vcpkg)**: libpqxx 7.9.2 (PostgreSQL), sqlite3 3.45.0+, mongo-cxx-driver 3.8.0+, hiredis 1.2.0+
**Dev/test**: Google Test 1.17.0, Google Benchmark 1.9.5

## Known Constraints

- Platform: Linux, macOS, Windows; UWP/Xbox excluded
- ProxyMode is stub-only (server-side pooling via future `database_server` not available)
- Local connection pooling removed (Phase 4.3) in anticipation of ProxyMode
- MongoDB and Redis backends are experimental (disabled by default, limited test coverage)
- Oracle backend: enum value exists but no implementation
- C++20 modules optional (CMake 3.28+), not default
- snake_case enforced by clang-tidy; private members use `_` suffix
