---
doc_id: "DBS-GUID-002"
doc_title: "Backend and Integration Feature Matrix"
doc_version: "1.1.0"
doc_date: "2026-05-31"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Backend and Integration Feature Matrix

> **SSOT**: This document is the single source of truth for **backend support
> status, ecosystem-integration support status, and the CMake-vs-vcpkg default
> behavior** of `database_system`.

This document records the maturity level and default-enablement behavior of every
database backend and ecosystem integration option, reconciles the CMake build
defaults against the vcpkg feature defaults, and documents the lifecycle of the
legacy `<database/...>` include shims. It also outlines the process for graduating
experimental backends to stable status.

---

## 1. Feature Matrix

The following matrix is the authoritative summary. Each row lists the controlling
CMake option (and its default), the vcpkg feature that pulls in the dependency
(and whether it is in the vcpkg **default-features** set), the support level, and a
command that verifies the feature configures.

| Feature / Integration | CMake option | CMake default | vcpkg feature | In vcpkg default-features | Support level | Verification command |
|---|---|:---:|---|:---:|---|---|
| PostgreSQL backend | `USE_POSTGRESQL` | **ON** | `postgresql` | **Yes** | Stable | `cmake --preset default` |
| SQLite backend | `USE_SQLITE` | OFF | `sqlite` | No | Stable | `cmake -B build -DUSE_SQLITE=ON` |
| MongoDB backend | `USE_MONGODB` | OFF | `mongodb` | No | **Experimental** | `cmake -B build -DUSE_MONGODB=ON` |
| Redis backend | `USE_REDIS` | OFF | `redis` | No | **Experimental** | `cmake -B build -DUSE_REDIS=ON` |
| OpenSSL (TLS / crypto) | `USE_OPENSSL` | **ON** | (pulled in by `postgresql`) | **Yes** (via `postgresql`) | Stable | `cmake --preset default` (warns if OpenSSL absent) |
| thread_system integration | `USE_THREAD_SYSTEM` | **ON** | `ecosystem` | No | Optional (graceful fallback) | `cmake -B build -DUSE_THREAD_SYSTEM=ON` |
| monitoring_system integration | `USE_MONITORING_SYSTEM` | **ON** | `ecosystem` | No | Optional (graceful fallback) | `cmake -B build -DUSE_MONITORING_SYSTEM=ON` |
| container_system integration | `USE_CONTAINER_SYSTEM` | **ON** | `ecosystem` | No | Optional (graceful fallback) | `cmake -B build -DUSE_CONTAINER_SYSTEM=ON` |
| logger_system integration | (auto, via adapters) | n/a | `ecosystem` | No | Optional (graceful fallback) | `cmake -B build -DUSE_THREAD_SYSTEM=ON` |
| common_system (Tier 0) | (required, auto) | n/a (`BUILD_WITH_COMMON_SYSTEM` set ON on detection) | `kcenon-common-system` (top-level dependency) | **Yes** (always) | Required | `cmake --preset default` (FATAL_ERROR if missing) |
| Legacy `<database/...>` include shims | `DATABASE_DISABLE_LEGACY_HEADERS` | OFF (shims **installed**) | n/a | n/a | Deprecated, removal in **2.0.0** | `cmake --preset default` then check `<prefix>/include/database/` |
| Legacy `database::` namespace alias | `DATABASE_DISABLE_LEGACY_NAMESPACE` | OFF (alias **active**) | n/a | n/a | Deprecated, removal in **2.0.0** | compile any source using `database::` against installed headers |

Notes:

- "In vcpkg default-features" reflects the `default-features` array of
  `vcpkg.json`, which currently contains only `postgresql`. Consumers therefore
  receive PostgreSQL (and its OpenSSL dependency) out of the box, and must opt in
  to every other backend and to ecosystem integration.
- The integration rows are gated by the `ecosystem` vcpkg feature, which installs
  `kcenon-thread-system`, `kcenon-logger-system`, `kcenon-container-system`, and
  `kcenon-monitoring-system`. With a plain `vcpkg install` (default features only)
  these packages are **not** installed, so the CMake integration options
  gracefully disable themselves at configure time (see Section 3).
- The `OpenSSL` row has **no dedicated vcpkg feature**: OpenSSL is a transitive
  dependency of the `postgresql` feature (`"openssl", "version>=": "3.3.0"`), so
  it is present whenever the default `postgresql` feature is installed.

---

## 2. CMake-vs-vcpkg Default Reconciliation

There is an intentional and documented mismatch between the CMake build defaults
and the vcpkg default-features set. **The two default surfaces serve different
audiences and are deliberately NOT aligned.**

| Option group | CMake default (direct / FetchContent build) | vcpkg default (`vcpkg install`) |
|---|---|---|
| PostgreSQL (`USE_POSTGRESQL`) | ON | included (`postgresql` is the only default feature) |
| OpenSSL (`USE_OPENSSL`) | ON | included transitively via `postgresql` |
| Ecosystem integration (`USE_THREAD_SYSTEM`, `USE_MONITORING_SYSTEM`, `USE_CONTAINER_SYSTEM`) | **ON** | **not included** (behind the `ecosystem` feature) |
| SQLite / MongoDB / Redis | OFF | not included (opt-in features) |

### Why they differ (intentional)

- **Direct CMake / FetchContent consumers** are typically building inside the
  kcenon monorepo or a superbuild where `thread_system`, `monitoring_system`, and
  `container_system` are already present. Defaulting the integration options to
  `ON` gives those consumers the full-featured build with no extra flags. When a
  sibling system is *not* present, the CMake logic degrades gracefully: each
  `USE_*_SYSTEM` option that cannot be satisfied is automatically forced `OFF`
  with a `STATUS`/`WARNING` message (see `cmake/dependencies.cmake`), so the
  default-ON posture never breaks a standalone configure.
- **vcpkg consumers** install `database_system` as a standalone port. Pulling the
  entire kcenon ecosystem (four additional registry packages) into every default
  install would be surprising and heavyweight, and those packages may not be
  present in a consumer's registry. The vcpkg default is therefore deliberately
  minimal — PostgreSQL only — and ecosystem integration is opt-in via the
  `ecosystem` feature: `vcpkg install kcenon-database-system[ecosystem]`.

### Net effect for consumers

| Build path | What you get by default |
|---|---|
| `cmake --preset default` (no vcpkg) | PostgreSQL + OpenSSL + ecosystem integration **if the sibling systems are findable**; integration auto-disables otherwise |
| `vcpkg install kcenon-database-system` | PostgreSQL + OpenSSL only; **no** ecosystem integration |
| `vcpkg install kcenon-database-system[ecosystem]` | PostgreSQL + OpenSSL + ecosystem integration packages installed |
| `cmake --preset full` | All four backends (PostgreSQL, SQLite, MongoDB, Redis) + integration |

This difference is **by design**; do not "fix" it by forcing the ecosystem
packages into the vcpkg default-features set or by flipping the CMake integration
options to OFF. If the policy ever changes, update this section, `vcpkg.json`, and
`cmake/options.cmake` together.

---

## 3. Graceful-Degradation Contract (ecosystem integrations)

The ecosystem integration options default to `ON` but never hard-fail a build:

```
USE_THREAD_SYSTEM=ON  -> find_system_dependency(thread_system)
                          found     -> integration enabled
                          not found -> WARNING + USE_THREAD_SYSTEM forced OFF
```

The same pattern applies to `USE_MONITORING_SYSTEM` and `USE_CONTAINER_SYSTEM`
(`cmake/dependencies.cmake`). Only `common_system` is mandatory: if it is not
found, configuration aborts with `FATAL_ERROR` because the `Result<T>` pattern is
required.

`USE_OPENSSL=ON` is also non-fatal when OpenSSL is absent: `secure_connection`
falls back to placeholder crypto with a loud `WARNING` (not for production). Pass
`-DUSE_OPENSSL=OFF` only for minimal embedded builds; see
`docs/compliance/ISO_27001.md`.

---

## 4. Backend Status Summary

| Backend          | Status         | Since   | vcpkg Feature    | Notes                                |
|------------------|----------------|---------|------------------|--------------------------------------|
| PostgreSQL       | **Stable**     | v0.1.0  | `postgresql`     | Primary backend, full feature set    |
| SQLite           | **Stable**     | v0.1.0  | `sqlite`         | Embedded / single-file database      |
| MongoDB          | Experimental   | v0.1.0  | `mongodb`        | Document-store backend, limited testing |
| Redis            | Experimental   | v0.1.0  | `redis`          | Key-value store backend, limited testing |

> Experimental status is also declared in `vcpkg.json` feature descriptions
> (e.g., `"Enable MongoDB backend (experimental)"`), in `cmake/options.cmake`
> (the `USE_MONGODB` / `USE_REDIS` option help strings and the configure-time
> `WARNING` messages), and in `README.md`. All of these sources must be updated
> together when a backend is promoted.

---

## 5. Stable Backends

### PostgreSQL

PostgreSQL is the primary backend and reference implementation for the
database\_system project.

- Full CRUD operations with parameterized queries
- Transaction management
- Comprehensive integration test coverage
- CI-validated on Linux, macOS, and Windows

> **External-dependency / external-service requirement**: `USE_POSTGRESQL`
> defaults to `ON`, but actual PostgreSQL support is compiled in **only when the
> `libpqxx` client library (and OpenSSL) are found**. If `libpqxx` is absent,
> `src/CMakeLists.txt` emits `WARNING "PostgreSQL libraries not found, disabling
> PostgreSQL support"` and force-sets `USE_POSTGRESQL=OFF` — the configure still
> succeeds. Verified locally: `cmake --preset default` with no vcpkg toolchain
> configures cleanly and reports `PostgreSQL support: OFF` because `libpqxx` was
> not on the system; the vcpkg `postgresql` default feature (or a system
> `libpqxx-dev`) provides it. A *running* PostgreSQL server is required only to
> execute integration tests or runtime connections, not to configure or build —
> the default configure does not contact any database server.

> **Note (Phase 4.3)**: Local client-side connection pooling has been removed. Production pooling is handled server-side via ProxyMode with `database_server` middleware. See [CHANGELOG](CHANGELOG.md) and [README](../README.md).

### SQLite

SQLite provides an embedded, zero-configuration database option.

- Full CRUD operations
- Single-file storage, no server required
- Ideal for testing, prototyping, and embedded deployments
- CI-validated on Linux, macOS, and Windows

---

## 6. Experimental Backends

> **Warning**: Experimental backends are **not recommended for production use**.
> APIs may change without notice in future releases. Their experimental status is
> surfaced at every introduction point: `vcpkg.json` feature descriptions,
> `cmake/options.cmake` option help strings and configure-time `WARNING`s,
> `README.md` / `README.kr.md`, and this document.

### MongoDB

**Status**: Experimental

MongoDB support provides a document-store backend using the MongoDB C++ driver.

**Current Limitations**:
- No dedicated integration test suite
- Test coverage is limited to basic connectivity and simple operations
- Connection pooling behavior has not been validated under load
- Error handling may not cover all driver-specific failure modes
- Mapping between relational query patterns and MongoDB queries is incomplete

**Known Issues**:
- Query translation from the relational API surface to MongoDB aggregation
  pipelines may produce unexpected results for complex queries
- Schema-less nature of MongoDB can cause silent data inconsistencies when
  used through the typed DAL interface

**API Stability**: None. The MongoDB-specific API surface may change in any
release without deprecation warnings.

### Redis

**Status**: Experimental

Redis support provides a key-value store backend using the Redis C++ client
library.

**Current Limitations**:
- No dedicated integration test suite
- Test coverage is limited to basic connectivity and simple operations
- Redis is an in-memory data store and is not designed for persistent
  relational data storage
- Connection pooling behavior has not been validated under load
- Error handling may not cover all client-specific failure modes

**Known Issues**:
- Data model mapping from relational tables to Redis key-value pairs is
  limited and may not support all query patterns
- No built-in support for complex queries, joins, or transactions
- Data persistence depends on Redis server configuration (RDB/AOF) and is
  not managed by the database\_system library

**API Stability**: None. The Redis-specific API surface may change in any
release without deprecation warnings.

---

## 7. Legacy Include Shim and Namespace Lifecycle

The pre-#591 public surface is preserved by two backward-compatibility mechanisms,
both **installed by default** and both **scheduled for removal in version 2.0.0**.
Because removing a public include path or a public namespace spelling is
source-breaking, removal is deferred to the next SemVer **major** release (current
version is 1.0.0).

| Compatibility surface | Controlling option | Default | Behavior | Removal target |
|---|---|:---:|---|:---:|
| Legacy `<database/...>` include shims (53 forwarding headers under `legacy_include/database/`) | `DATABASE_DISABLE_LEGACY_HEADERS` | OFF (installed) | Each shim emits a `#pragma message` redirecting to `<kcenon/database/...>` and includes the canonical header | **2.0.0** |
| Legacy `database::` namespace alias (`kcenon/database/compat.h`) | `DATABASE_DISABLE_LEGACY_NAMESPACE` | OFF (alias active) | `namespace database = kcenon::database;` keeps the old spelling compiling | **2.0.0** |

- Consumers that have migrated to `<kcenon/database/...>` and `kcenon::database::`
  can set `DATABASE_DISABLE_LEGACY_HEADERS=ON` and/or
  `DATABASE_DISABLE_LEGACY_NAMESPACE` to build without the compatibility surface.
- Both surfaces are intended to be removed together in the **2.0.0** breaking
  window so consumers migrate include path and namespace in one step.
- See issues [#582](https://github.com/kcenon/database_system/issues/582) and
  [#591](https://github.com/kcenon/database_system/issues/591) and the CHANGELOG
  "Deprecated" section for the full rationale.

---

## 8. Stabilization Criteria

Before an experimental backend can be promoted to **stable**, all of the
following criteria must be met:

| #  | Criterion                          | Description                                                      |
|----|------------------------------------|------------------------------------------------------------------|
| 1  | Dedicated integration test suite   | A standalone test suite with **>80% code coverage** of the backend module |
| 2  | Performance benchmarks             | Documented baseline benchmarks for common operations (insert, select, update, delete) |
| 3  | Connection pooling validation      | Connection pool creation, reuse, and teardown verified under concurrent load |
| 4  | Error handling completeness audit  | All driver/client error codes mapped to database\_system error types with appropriate recovery or reporting |
| 5  | CI platform coverage               | Tests passing on **Linux, macOS, and Windows** in the CI pipeline |
| 6  | Production deployment reference    | At least **one documented production deployment** demonstrating real-world viability |

### Stabilization Progress

Current status of each experimental backend against the stabilization
criteria listed above:

| Criterion                        | MongoDB | Redis |
|----------------------------------|:-------:|:-----:|
| Dedicated integration test suite | --      | --    |
| Performance benchmarks           | --      | --    |
| Connection pooling validation    | --      | --    |
| Error handling audit             | --      | --    |
| CI platform coverage             | --      | --    |
| Production deployment reference  | --      | --    |

Legend: done = met, partial = in progress, -- = not started.

---

## 9. Stabilization Roadmap

### Phase 1 -- Documentation and Clarity (current)

- Mark experimental backends clearly in CMake configuration and documentation
- Publish this document (`docs/BACKENDS.md`) as the single source of truth
- Emit CMake warnings when experimental backends are enabled

### Phase 2 -- Test Coverage Expansion

- Create dedicated integration test suites for MongoDB and Redis
- Achieve >80% code coverage for each experimental backend module
- Add negative / error-path tests for driver-specific failure modes

### Phase 3 -- API Stability Lock

- Review and finalize the public API surface for each backend
- Align MongoDB and Redis APIs with the stable backend interface contract
- Document any intentional deviations from the relational API pattern
- Publish migration notes for any breaking changes

### Phase 4 -- Production Hardening and Graduation

- Run performance benchmarks and publish baseline numbers
- Validate connection pooling under realistic concurrent workloads
- Collect at least one production deployment reference per backend
- Promote backend status from **Experimental** to **Stable**
- Remove CMake experimental warnings

---

## 10. Backend Comparison Table

| Feature                    | PostgreSQL | SQLite | MongoDB      | Redis        |
|----------------------------|:----------:|:------:|:------------:|:------------:|
| Stability                  | Stable     | Stable | Experimental | Experimental |
| CRUD operations            | Yes        | Yes    | Partial      | Partial      |
| Parameterized queries      | Yes        | Yes    | N/A          | N/A          |
| Connection pooling         | Yes        | N/A    | Unvalidated  | Unvalidated  |
| Transaction support        | Yes        | Yes    | Limited      | No           |
| Dedicated test suite       | Yes        | Yes    | No           | No           |
| CI coverage (all platforms)| Yes        | Yes    | No           | No           |
| Performance benchmarks     | Yes        | Yes    | No           | No           |
| Production references      | Yes        | Yes    | No           | No           |
| Server required            | Yes        | No     | Yes          | Yes          |
| Data model                 | Relational | Relational | Document  | Key-Value    |
