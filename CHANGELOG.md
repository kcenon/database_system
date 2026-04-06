# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- **BREAKING**: Unified CMake package name from `DatabaseSystem` to `database_system` for vcpkg consumption; use `find_package(database_system CONFIG REQUIRED)` ([#547](https://github.com/kcenon/database_system/issues/547))

### Documentation

- Modernize Doxygen with doxygen-awesome-css theme, dark mode toggle, and standardized mainpage ([#537](https://github.com/kcenon/database_system/issues/537))

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
