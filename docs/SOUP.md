# SOUP List &mdash; database_system

> **Software of Unknown Provenance (SOUP) Register per IEC 62304:2006+AMD1:2015 &sect;8.1.2**
>
> This document is the authoritative reference for all external software dependencies.
> Every entry must include: title, manufacturer, unique version identifier, license, and known anomalies.

| Document | Version |
|----------|---------|
| IEC 62304 Reference | &sect;8.1.2 Software items from SOUP |
| Last Reviewed | 2026-03-06 |
| database_system Version | 1.0.0 |

---

## Internal Ecosystem Dependencies (Optional `ecosystem` Feature)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| INT-001 | [common_system](https://github.com/kcenon/common_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Result&lt;T&gt; pattern, error handling primitives | B | None |
| INT-002 | [thread_system](https://github.com/kcenon/thread_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Thread pool, async task scheduling | B | None |
| INT-003 | [logger_system](https://github.com/kcenon/logger_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Structured logging infrastructure | A | None |
| INT-004 | [container_system](https://github.com/kcenon/container_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Serializable data containers | B | None |
| INT-005 | [monitoring_system](https://github.com/kcenon/monitoring_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Performance metrics collection | A | None |

> **Note**: Ecosystem dependencies are enabled via the optional `ecosystem` vcpkg feature. database_system can operate independently with only fmt and ASIO.

---

## Production SOUP (Required)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-001 | [fmt](https://github.com/fmtlib/fmt) | Victor Zverovich | 10.2.1 | MIT | String formatting for SQL query building and error messages | A | None |
| SOUP-002 | [ASIO](https://github.com/chriskohlhoff/asio) (standalone) | Christopher Kohlhoff | 1.30.2 | BSL-1.0 | Async I/O for database connection management | B | None |

### System Dependencies

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-003 | POSIX Threads (pthreads) | POSIX / OS vendor | System-provided | N/A (OS) | Concurrent database operations via `find_package(Threads)` | B | None |

---

## Optional SOUP &mdash; Database Backends

### PostgreSQL Feature (`postgresql`)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-004 | [libpq](https://www.postgresql.org/) | PostgreSQL Global Development Group | 16.2 | PostgreSQL License | PostgreSQL C client library | B | None |
| SOUP-005 | [libpqxx](https://github.com/jtv/libpqxx) | Jeroen T. Vermeulen | 7.9.0 | BSD-3-Clause | PostgreSQL C++ client wrapper | B | None |
| SOUP-006 | [OpenSSL](https://www.openssl.org/) | OpenSSL Project | 3.3.0 | Apache-2.0 | TLS encryption for PostgreSQL connections | C | CVE tracking via vendor advisories required |

### MySQL Feature (`mysql`)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-007 | [libmysql](https://dev.mysql.com/doc/c-api/en/) | Oracle Corporation | 8.0.34 | GPL-2.0 (with FOSS exception) | MySQL C client library | B | GPL with FOSS License Exception allows linking with BSD-licensed code |

### SQLite Feature (`sqlite`)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-008 | [SQLite](https://www.sqlite.org/) | SQLite Consortium | 3.45.3 | Public Domain | Embedded SQL database engine | B | None |

### MongoDB Feature (`mongodb`) &mdash; Experimental

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-009 | [mongo-cxx-driver](https://github.com/mongodb/mongo-cxx-driver) | MongoDB, Inc. | 3.10.1 | Apache-2.0 | MongoDB C++ client driver (experimental) | B | Experimental backend; not recommended for production clinical data |

### Redis Feature (`redis`) &mdash; Experimental

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-010 | [hiredis](https://github.com/redis/hiredis) | Redis Ltd. | 1.2.0 | BSD-3-Clause | Minimalistic Redis C client (experimental) | A | Experimental cache backend; not intended for persistent clinical data |

### Logging Feature (`logging`)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-011 | [spdlog](https://github.com/gabime/spdlog) | Gabi Melman | 1.13.0 | MIT | Advanced logging capabilities with fmt-external support | A | None |

---

## Development/Test SOUP (Not Deployed)

| ID | Name | Manufacturer | Version | License | Usage | Qualification |
|----|------|-------------|---------|---------|-------|--------------|
| SOUP-T01 | [Google Test](https://github.com/google/googletest) | Google | 1.14.0 | BSD-3-Clause | Unit testing framework (includes GMock) | Required |
| SOUP-T02 | [Google Benchmark](https://github.com/google/benchmark) | Google | 1.8.3 | Apache-2.0 | Performance benchmarking framework | Not required |

---

## Safety Classification Key

| Class | Definition | Example |
|-------|-----------|---------|
| **A** | No contribution to hazardous situation | Logging, formatting, test frameworks |
| **B** | Non-serious injury possible | Data processing, network communication |
| **C** | Death or serious injury possible | Encryption, access control |

---

## Version Pinning (IEC 62304 Compliance)

All SOUP versions are pinned in `vcpkg.json` via the `overrides` field:

```json
{
  "overrides": [
    { "name": "fmt", "version": "10.2.1" },
    { "name": "asio", "version": "1.30.2" },
    { "name": "openssl", "version": "3.3.0" },
    { "name": "libpq", "version": "16.2" },
    { "name": "libpqxx", "version": "7.9.0" },
    { "name": "libmysql", "version": "8.0.34" },
    { "name": "sqlite3", "version": "3.45.3" },
    { "name": "mongo-cxx-driver", "version": "3.10.1" },
    { "name": "hiredis", "version": "1.2.0" },
    { "name": "spdlog", "version": "1.13.0" },
    { "name": "gtest", "version": "1.14.0" },
    { "name": "benchmark", "version": "1.8.3" }
  ]
}
```

The vcpkg baseline is locked in `vcpkg-configuration.json` to ensure reproducible builds.

---

## Version Update Process

When updating any SOUP dependency:

1. Update the version in `vcpkg.json` (overrides section)
2. Update the corresponding row in this document
3. Verify no new known anomalies (check CVE databases, especially for OpenSSL and database drivers)
4. Run full CI/CD pipeline to confirm compatibility
5. Document the change in the PR description

---

## License Compliance Summary

| License | Count | Copyleft | Obligation |
|---------|-------|----------|------------|
| MIT | 2 | No | Include copyright notice |
| BSL-1.0 | 1 | No | Include license |
| Apache-2.0 | 2 | No | Include license + NOTICE file |
| BSD-3-Clause | 3 | No | Include copyright + no-endorsement clause |
| Public Domain | 1 | No | None |
| PostgreSQL License | 1 | No | Include copyright notice |
| GPL-2.0 (FOSS exception) | 1 | Conditional | FOSS License Exception permits linking with BSD-3-Clause |

> **GPL contamination**: libmysql (SOUP-007) uses GPL-2.0 with FOSS License Exception, which permits use with BSD-3-Clause licensed projects. Verify FOSS exception applicability when distributing.
