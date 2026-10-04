# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Thread-safe `connection_pool` with RAII `pooled_connection` lease handle, bounded
  sizing, acquisition timeout, and transparent replacement of broken connections.
  The pool is backend-agnostic and lives alongside existing direct-connection APIs
  without breaking them ([#568](https://github.com/kcenon/database_system/issues/568)).
- Public forwarding header `kcenon/database/core/connection_pool.h`.
- Unit test suite `connection_pool_test` covering pre-warming, concurrent
  checkout/checkin, pool exhaustion, broken-connection replacement, and shutdown.
- Compatibility header `kcenon/database/compat.h` declaring the global namespace
  alias `namespace database = kcenon::database;`. Pulled in by the foundation
  headers so pre-#591 source using `database::` (or `::database::`) keeps
  compiling. Define `DATABASE_DISABLE_LEGACY_NAMESPACE` to build without the
  alias ([#591](https://github.com/kcenon/database_system/issues/591)).

### Changed

- **BREAKING:** Remapped every non-zero `kcenon::database::error_code` enumerator
  into common_system's reserved `database_system` band `[-599, -500]`. The old
  values collided with common's `common_errors` band (`-1..-99`): casting the
  former `connection_failed = -5` into the shared `kcenon::common::error_info::code`
  made consumers resolve it as `Cancelled` / category `Common` instead of
  `DatabaseSystem`. New values: `success = 0` (unchanged),
  `connection_failed = -500` (aligned to
  `common::error::codes::database_system::connection_failed`),
  `query_failed = -540` (aligned to `database_system::query_failed`),
  `timeout = -542` (aligned to `database_system::query_timeout`),
  `invalid_state = -596`, `not_implemented = -597`, `invalid_argument = -598`,
  `unknown_error = -599`. In addition, every hand-coded error code that previously
  fed `kcenon::common::error_info` with a positive or `-1..-99` value (backend
  managers, `backend_registry`, integrated adapters, coordinator, and unified
  system) is now routed through these in-band codes, so the shared error registry
  attributes them to the `DatabaseSystem` category. Any consumer comparing against
  the old numeric values must update. This is a SemVer-major break for callers that
  read raw `error_info::code`; it must ship as **v1.1.0 coordinated with the
  ecosystem** — the version bump and release are owner-coordinated and are
  intentionally not performed here
  ([#600](https://github.com/kcenon/database_system/issues/600)).
- Internal namespace migrated from `database::` to `kcenon::database::` so the
  namespace matches the canonical `kcenon/database/...` include path. All in-tree
  namespace definitions (headers, sources, and C++20 module partitions) now open
  `kcenon::database`; a tracked backward-compatibility alias keeps the legacy
  `database::` spelling working for existing consumers
  ([#591](https://github.com/kcenon/database_system/issues/591)).
- Relocated the loose `*_manager` / query translation units out of the `src` root
  into their domain folders: `database_manager.cpp` -> `core/`,
  `postgres_manager.cpp` -> `backends/`, `query_builder.cpp` and
  `query_dialect.cpp` -> `query_builder/`. Public include paths are unchanged
  ([#591](https://github.com/kcenon/database_system/issues/591)).

### Deprecated

- Header path `<database/...>` is deprecated; use `<kcenon/database/...>`.
  Forwarding stubs at the legacy paths emit `#pragma message` warnings and
  include the canonical headers. Set `DATABASE_DISABLE_LEGACY_HEADERS=ON` to
  opt out. **Stubs will be removed in version 2.0.0** (removing a public include
  path is a breaking change, hence a major bump)
  ([#582](https://github.com/kcenon/database_system/issues/582),
  part of [#577](https://github.com/kcenon/database_system/issues/577)).
- Namespace `database::` is deprecated; use `kcenon::database::`. The compat
  alias in `kcenon/database/compat.h` keeps the old spelling working. **The alias
  will be removed in version 2.0.0**, in the same breaking-change window as the
  legacy `<database/...>` include shims, so consumers migrate include path and
  namespace together ([#591](https://github.com/kcenon/database_system/issues/591)).

### Documentation

- Reworked `docs/BACKENDS.md` into the authoritative **backend and integration
  feature matrix**: each backend, ecosystem integration, OpenSSL, and the legacy
  include shims now lists its CMake option/default, vcpkg feature and
  default-features membership, support level, and a verification command. Added a
  dedicated section reconciling the **CMake-vs-vcpkg default mismatch** (the two
  default surfaces are intentionally different: direct CMake defaults ecosystem
  integration ON with graceful fallback, while the vcpkg default ships only the
  `postgresql` feature). Documented the legacy `<database/...>` shim and
  `database::` namespace-alias lifecycle (both removed in **2.0.0**). Linked the
  matrix from `README.md`, `README.kr.md`, and `docs/advanced/CURRENT_STATE.md`,
  extended the README build-option tables with the integration/OpenSSL/legacy
  options, and surfaced MongoDB/Redis experimental status at every introduction
  point ([#590](https://github.com/kcenon/database_system/issues/590)).

## [1.0.0] - 2026-04-16

### Changed

- **BREAKING**: Unified CMake package name from `DatabaseSystem` to `database_system` for vcpkg consumption; use `find_package(database_system CONFIG REQUIRED)` ([#547](https://github.com/kcenon/database_system/issues/547))
- **BREAKING**: `unified_database_system::builder::build()` now returns `Result<std::unique_ptr<unified_database_system>>` instead of throwing on failure ([#564](https://github.com/kcenon/database_system/issues/564))
- `unified_database_system` constructor defers coordinator initialization to `connect()` for no-throw guarantee ([#564](https://github.com/kcenon/database_system/issues/564))
- All synchronous public APIs now use `Result<T>` for error propagation with zero `throw` statements ([#564](https://github.com/kcenon/database_system/issues/564))

### Documentation

- Modernize Doxygen with doxygen-awesome-css theme, dark mode toggle, and standardized mainpage ([#537](https://github.com/kcenon/database_system/issues/537))
- Add v1.0 migration guide in README for CMake and builder API changes ([#564](https://github.com/kcenon/database_system/issues/564))

### Security

- Implement credential encryption and audit log file persistence ([#485](https://github.com/kcenon/database_system/issues/485), [#486](https://github.com/kcenon/database_system/issues/486))

### Performance

- Add `execute_batch()` for PostgreSQL batch transaction mode ([#487](https://github.com/kcenon/database_system/issues/487))

## [0.1.0] - 2026-03-14

### Added
- Multi-backend database abstraction (PostgreSQL, SQLite, MongoDB, Redis)
- Type-safe query builder with fluent API
- ORM framework with entity mapping
- Real-time performance monitoring
- Connection pooling with health checks
- Async operations via Asio
- Enterprise security features (encrypted connections, audit logging)
- Facade pattern for simplified database access
- Feature-based vcpkg backend selection (#440)
- CMake install/export infrastructure for find_package (#429)
- Release workflow for v0.1.0 tagged releases (#444)
- C++20 module support
- IEC 62304 SOUP compliance documentation

### Infrastructure
- GitHub Actions CI/CD with sanitizer testing
- Doxygen documentation workflow
- vcpkg manifest with backend-specific features
- codecov.io integration
- Cross-platform support (Linux, macOS, Windows)

[Unreleased]: https://github.com/kcenon/database_system/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/kcenon/database_system/compare/v0.1.0...v1.0.0
[0.1.0]: https://github.com/kcenon/database_system/releases/tag/v0.1.0
