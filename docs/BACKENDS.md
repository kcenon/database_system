# Backend Stability Overview

This document describes the maturity level of each database backend in the
database\_system project and outlines the process for graduating experimental
backends to stable status.

## Status Summary

| Backend          | Status         | Since   | Notes                                |
|------------------|----------------|---------|--------------------------------------|
| PostgreSQL       | **Stable**     | v0.1.0  | Primary backend, full feature set    |
| MySQL / MariaDB  | **Stable**     | v0.1.0  | Full feature parity with PostgreSQL  |
| SQLite           | **Stable**     | v0.1.0  | Embedded / single-file database      |
| MongoDB          | Experimental   | v0.1.0  | Document-store backend, limited testing |
| Redis            | Experimental   | v0.1.0  | Key-value store backend, limited testing |

---

## Stable Backends

### PostgreSQL

PostgreSQL is the primary backend and reference implementation for the
database\_system project.

- Full CRUD operations with parameterized queries
- Connection pooling support
- Transaction management
- Comprehensive integration test coverage
- CI-validated on Linux, macOS, and Windows

### MySQL / MariaDB

MySQL and MariaDB are supported as stable backends with full feature parity.

- Full CRUD operations with parameterized queries
- Connection pooling support
- Transaction management
- Tested against MySQL 8.x and MariaDB 10.x+
- CI-validated on Linux, macOS, and Windows

### SQLite

SQLite provides an embedded, zero-configuration database option.

- Full CRUD operations
- Single-file storage, no server required
- Ideal for testing, prototyping, and embedded deployments
- CI-validated on Linux, macOS, and Windows

---

## Experimental Backends

> **Warning**: Experimental backends are **not recommended for production use**.
> APIs may change without notice in future releases.

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

## Stabilization Criteria

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

---

## Stabilization Roadmap

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

## Backend Comparison Table

| Feature                    | PostgreSQL | MySQL / MariaDB | SQLite | MongoDB      | Redis        |
|----------------------------|:----------:|:---------------:|:------:|:------------:|:------------:|
| Stability                  | Stable     | Stable          | Stable | Experimental | Experimental |
| CRUD operations            | Yes        | Yes             | Yes    | Partial      | Partial      |
| Parameterized queries      | Yes        | Yes             | Yes    | N/A          | N/A          |
| Connection pooling         | Yes        | Yes             | N/A    | Unvalidated  | Unvalidated  |
| Transaction support        | Yes        | Yes             | Yes    | Limited      | No           |
| Dedicated test suite       | Yes        | Yes             | Yes    | No           | No           |
| CI coverage (all platforms)| Yes        | Yes             | Yes    | No           | No           |
| Performance benchmarks     | Yes        | Yes             | Yes    | No           | No           |
| Production references      | Yes        | Yes             | Yes    | No           | No           |
| Server required            | Yes        | Yes             | No     | Yes          | Yes          |
| Data model                 | Relational | Relational      | Relational | Document  | Key-Value    |
