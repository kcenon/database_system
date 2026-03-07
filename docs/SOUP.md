# SOUP List &mdash; database_system

> **Software of Unknown Provenance (SOUP) Register per IEC 62304:2006+AMD1:2015 &sect;8.1.2**
>
> This document is the authoritative reference for all external software dependencies.
> Every entry must include: title, manufacturer, unique version identifier, license, and known anomalies.

| Document | Version |
|----------|---------|
| IEC 62304 Reference | &sect;8.1.2 Software items from SOUP |
| Last Reviewed | 2026-03-07 |
| database_system Version | 0.1.0 |

---

## Production SOUP

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Linking | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|---------|-----------------|
| SOUP-001 | [Standalone Asio](https://think-async.com/Asio/) | Christopher Kohlhoff | 1.30.2 | BSL-1.0 | Asynchronous I/O for database connection pooling (core dependency) | B | Header-only | None |

---

## Optional SOUP

### Database Backends

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Linking | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|---------|-----------------|
| SOUP-002 | [libpq](https://www.postgresql.org/) | PostgreSQL Global Development Group | 16.2 | PostgreSQL | PostgreSQL C client library (`postgresql` feature) | B | Dynamic | None |
| SOUP-003 | [libpqxx](https://pqxx.org/) | Jeroen T. Vermeulen | 7.9.0 | BSD-3-Clause | PostgreSQL C++ wrapper (`postgresql` feature) | B | Dynamic | None |
| SOUP-004 | [OpenSSL](https://www.openssl.org/) | OpenSSL Software Foundation | 3.3.0 | Apache-2.0 | TLS encryption for PostgreSQL connections (`postgresql` feature) | C | Dynamic | None known at pinned version |
| SOUP-005 | [MariaDB Connector/C](https://mariadb.com/kb/en/mariadb-connector-c/) | MariaDB Corporation | 3.3.8 | LGPL-2.1-or-later | MySQL/MariaDB database connectivity (`mysql` feature) | B | **Dynamic only** (LGPL) | None |
| SOUP-006 | [SQLite](https://www.sqlite.org/) | D. Richard Hipp | 3.45.3 | Public Domain | Embedded SQL database (`sqlite` feature) | B | Static or dynamic | None |
| SOUP-007 | [MongoDB C++ Driver](https://github.com/mongodb/mongo-cxx-driver) | MongoDB, Inc. | 3.10.1 | Apache-2.0 | MongoDB database connectivity (`mongodb` feature, experimental) | B | Dynamic | None |
| SOUP-008 | [Hiredis](https://github.com/redis/hiredis) | Redis Ltd. | 1.2.0 | BSD-3-Clause | Redis client library (`redis` feature, experimental) | A | Dynamic | None |

### Utility Libraries

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Linking | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|---------|-----------------|
| SOUP-009 | [spdlog](https://github.com/gabime/spdlog) | Gabi Melman | 1.13.0 | MIT | Fast C++ logging library (`logging` feature) | A | Header-only or shared | None |

> **LGPL Compliance**: MariaDB Connector/C (SOUP-005) must be dynamically linked to preserve BSD-3-Clause licensing. vcpkg triplet overlay forces dynamic linking. CMake build-time check and CI `ldd`/`otool -L` verification ensure compliance.

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
| **A** | No contribution to hazardous situation | Logging, caching, formatting |
| **B** | Non-serious injury possible | Database connectivity, data processing |
| **C** | Death or serious injury possible | Encryption, access control |

---

## Version Pinning (IEC 62304 Compliance)

All SOUP versions are pinned in `vcpkg.json` via the `overrides` field:

```json
{
  "overrides": [
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
3. Verify no new known anomalies (check CVE databases)
4. Run full CI/CD pipeline to confirm compatibility
5. Document the change in the PR description

---

## License Compliance Summary

| License | Count | Copyleft | Obligation |
|---------|-------|----------|------------|
| BSD-3-Clause | 3 | No | Include copyright + no-endorsement clause |
| Apache-2.0 | 3 | No | Include license + NOTICE file |
| BSL-1.0 | 1 | No | Include license text |
| PostgreSQL | 1 | No | Include copyright notice |
| Public Domain | 1 | None | No obligations |
| MIT | 1 | No | Include copyright notice |
| LGPL-2.1-or-later | 1 | Weak | Dynamic linking required; include license text |

> **LGPL contamination**: Avoided by mandatory dynamic linking of MariaDB Connector/C. vcpkg triplet overlay enforces `dynamic` linkage. See LICENSE-THIRD-PARTY for details.
