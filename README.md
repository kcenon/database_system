[![CI](https://github.com/kcenon/database_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/database_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/database_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/static-analysis.yml)
[![codecov](https://codecov.io/gh/kcenon/database_system/branch/main/graph/badge.svg)](https://codecov.io/gh/kcenon/database_system)
[![Documentation](https://github.com/kcenon/database_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/build-Doxygen.yaml)
[![License](https://img.shields.io/github/license/kcenon/database_system)](https://github.com/kcenon/database_system/blob/main/LICENSE)

# Database System

> **Language:** **English** | [한국어](README.kr.md)

## Table of Contents

- [Overview](#overview)
- [Requirements](#requirements)
- [Core Features](#core-features)
- [Performance Highlights](#performance-highlights)
- [Quick Start](#quick-start)
- [Architecture Overview](#architecture-overview)
- [Ecosystem Integration](#ecosystem-integration)
- [Documentation](#documentation)
- [CMake Integration](#cmake-integration)
- [Production Quality](#production-quality)
- [Performance Baselines](#performance-baselines)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

A modern C++20 database abstraction layer providing unified access to multiple database backends with advanced features including ORM framework, real-time performance monitoring, enterprise security, and asynchronous operations.

**Key Value Proposition**: Eliminate vendor lock-in, maximize performance, and accelerate development with a comprehensive database solution that supports PostgreSQL, SQLite, MongoDB, and Redis through a unified, type-safe interface.

> **Connection Mode Status**:
> - **DirectMode**: Production-ready (stable)
> - **ProxyMode**: Stub implementation (awaiting `database_server`, not yet available)
>
> Currently, DirectMode is the only production option. Connection pooling has been removed locally (Phase 4.3) in preparation for server-side pooling via ProxyMode. See [migration guide](docs/migration/database_base.md) for details. <!-- TODO: dedicated proxy-mode.md migration doc -->

### v1.0.0 Release (2026-04)

- **API Frozen**: All synchronous public APIs use `Result<T>` for error propagation — no `throw` in public API paths
- **BREAKING**: `builder::build()` now returns `Result<std::unique_ptr<unified_database_system>>` instead of raw pointer
- **BREAKING**: CMake package name unified to `database_system`; use `find_package(database_system CONFIG REQUIRED)`
- **Migration from 0.x**: Update `builder::build()` call sites to handle `Result` (see [CHANGELOG](CHANGELOG.md))

### Previous Updates (2026-01)

- **C++20 Module Support**: Added module files for modern C++20 module imports
  - Primary module: `kcenon.database`
  - Module partitions: `:core`, `:query`, `:backends`
  - Requires CMake 3.28+ with `DATABASE_BUILD_MODULES=ON`
  - Compatible with existing header-based includes

### Previous Updates (2025-12)

- **[BREAKING] Connection Pooling Removed (Phase 4.3)**: Migration to ProxyMode completed
  - All local pooling classes removed: `connection_pool`, `connection_pool_v2`, `connection_pool_v3`
  - Resilience classes removed: `connection_health_monitor`, `resilient_database_connection`
  - Direct database connections are the current production approach
- **C++20 Concepts Integration**: Compile-time type validation for async operations
  - `SubmittableTask` concept for `submit()` methods
  - `ErrorHandler`, `QueryCallback` concepts for callbacks
  - `StreamEventHandler`, `StreamEventFilter` concepts for stream processing
  - `TransactionAction`, `CompensationAction` concepts for saga pattern
  - Clearer error messages and better IDE support
  - Backward compatible with existing `std::function` APIs
- **monitoring_system Integration**: Full integration for production-grade metrics collection
- **Immutable Query Builder**: Thread-safe query construction with functional programming style
- **ProxyMode Pooling**: Server-side connection pooling via database_server middleware (replaced local pooling)
- All CI/CD pipelines green across platforms

---

## Requirements

| Dependency | Version | Required | Description |
|------------|---------|----------|-------------|
| C++20 Compiler | GCC 13+ / Clang 17+ / MSVC 2022+ / Apple Clang 14+ | Yes | C++20 features required (see note below) |
| CMake | 3.20+ | Yes | Build system |
| [common_system](https://github.com/kcenon/common_system) | latest | Yes | Common interfaces and Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | latest | Optional | Thread pool for async operations (USE_THREAD_SYSTEM) |
| [logger_system](https://github.com/kcenon/logger_system) | latest | Optional | Logging via ILogger interface |
| [container_system](https://github.com/kcenon/container_system) | latest | Optional | Data serialization (USE_CONTAINER_SYSTEM) |
| [monitoring_system](https://github.com/kcenon/monitoring_system) | latest | Optional | Performance metrics (USE_MONITORING_SYSTEM) |

> **Note**: The GCC 13+ / Clang 17+ requirement comes from [thread_system](https://github.com/kcenon/thread_system), which is enabled by default (`USE_THREAD_SYSTEM=ON`). If you disable all optional ecosystem dependencies, GCC 11+ / Clang 14+ may suffice for core-only builds. See [thread_system requirements](https://github.com/kcenon/thread_system#requirements) for details.

### Database Backends (at least one required)

| Backend | Version | Optional Package |
|---------|---------|------------------|
| PostgreSQL | 12+ | `libpq-dev` |
| SQLite | 3.35+ | `libsqlite3-dev` |
| MongoDB | 5.0+ | `libmongoc-dev` |
| Redis | 6.0+ | `libhiredis-dev` |

### Dependency Flow

```
database_system
├── common_system (required)
├── thread_system (optional, USE_THREAD_SYSTEM=ON)
│   └── common_system
├── logger_system (optional, via ILogger interface)
│   └── common_system
├── container_system (optional, USE_CONTAINER_SYSTEM=ON)
│   └── common_system
└── monitoring_system (optional, USE_MONITORING_SYSTEM=ON)
    └── common_system, thread_system
```

### Building with Dependencies

```bash
# Clone all dependencies
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/logger_system.git
git clone https://github.com/kcenon/container_system.git
git clone https://github.com/kcenon/monitoring_system.git
git clone https://github.com/kcenon/database_system.git

# Build database_system
cd database_system
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_POSTGRESQL=ON
cmake --build build
```

### C++20 Module Support

For C++20 module-based development (requires CMake 3.28+):

```bash
# Build with module support
cmake -B build -DCMAKE_BUILD_TYPE=Release -DDATABASE_BUILD_MODULES=ON
cmake --build build
```

```cpp
// Using C++20 modules
import kcenon.database;

using namespace database;

auto context = std::make_shared<database_context>();
auto manager = std::make_shared<database_manager>(context);
manager->set_mode(database_types::sqlite);

auto builder = manager->create_query_builder();
auto query = builder
    .select({"id", "name"})
    .from("users")
    .where("active", "=", true)
    .build();
```

📖 **[Quick Start Guide →](docs/guides/QUICK_START.md)** | **[빠른 시작 가이드 →](docs/guides/QUICK_START.kr.md)**

---

## Core Features

### Multi-Backend Support

| Database | Status | Key Features | Performance |
|----------|--------|--------------|-------------|
| **PostgreSQL** | ✅ Full | JSONB, Arrays, CTEs, FTS | 1.2ms SELECT, 5K TPS |
| **SQLite** | ✅ Full | WAL mode, FTS5, In-memory | 0.8ms SELECT |
| **MongoDB** | 🧪 Experimental | Documents, Aggregation, GridFS | 2.1ms insertOne |
| **Redis** | 🧪 Experimental | All data types, Pub/Sub, Lua | 0.3ms GET/SET |

[📚 Detailed Backend Features →](docs/FEATURES.md)

### Experimental Features

> ⚠️ **Note**: The following backends are experimental and disabled by default.
> These backends are fully functional but may have limited support or undergo breaking changes in future releases.

| Backend | CMake Option | vcpkg Feature | Status | Notes |
|---------|--------------|---------------|--------|-------|
| **MongoDB** | `USE_MONGODB=ON` | `mongodb` | 🧪 Experimental | NoSQL document store |
| **Redis** | `USE_REDIS=ON` | `redis` | 🧪 Experimental | In-memory data store |

**To enable experimental backends:**

```bash
# Enable MongoDB support
cmake -DUSE_MONGODB=ON ..

# Enable Redis support
cmake -DUSE_REDIS=ON ..

# Enable both
cmake -DUSE_MONGODB=ON -DUSE_REDIS=ON ..
```

**vcpkg features (optional):**
```bash
# Install with specific features
vcpkg install database-system[mongodb,redis]
```

For detailed build instructions, see [Build Guide →](docs/guides/BUILD_GUIDE.md#experimental-backends)

### Quick Start — Database Connection

```cpp
#include <database/database_manager.h>

auto context = std::make_shared<database_context>();
auto db = std::make_shared<database_manager>(context);
db->set_mode(database_types::postgres);
db->connect("host=localhost port=5432 dbname=mydb");
```

### Type-Safe Query Builders

**Immutable Query Builder** (thread-safe, zero race conditions):

```cpp
#include <database/query/immutable_query_builder.h>

const auto base_query = immutable_query_builder()
    .select({"id", "name", "email"})
    .from("users");

// Branch 1: Active users
const auto active_users = base_query
    .where("is_active", "=", database_value{true})
    .order_by("name");

// Branch 2: Admin users (base_query unchanged)
const auto admin_users = base_query
    .where("role", "=", database_value{std::string("admin")})
    .order_by("created_at", sort_order::desc);

// Thread-safe execution
auto result1 = active_users.execute(&db);
auto result2 = admin_users.execute(&db);
```

**SQL and NoSQL Support**:
```cpp
// PostgreSQL
auto sql_query = db.create_query_builder(database_types::postgres)
    .select({"u.id", "u.username", "COUNT(p.id) as post_count"})
    .from("users u")
    .join("posts p", "u.id = p.user_id", join_type::left)
    .group_by("u.id", "u.username")
    .having("COUNT(p.id)", ">", database_value{int64_t(5)})
    .order_by("post_count", sort_order::desc)
    .limit(20);

// MongoDB
auto mongo_query = db.create_query_builder(database_types::mongodb)
    .collection("users")
    .aggregate({
        {"$match", {{"status", database_value{std::string("active")}}}},
        {"$group", {{"_id", "$department"}, {"total", {{"$sum", "$salary"}}}}}
    });

// Redis
auto redis_query = db.create_query_builder(database_types::redis)
    .hset("user:1000", {{"username", "john"}, {"email", "john@example.com"}});
```

[📘 Complete Query Builder Guide →](docs/FEATURES_ORM_QUERY.md#query-builders)

### ORM Framework (C++20 Concepts-based)

```cpp
#include <database/orm/entity.h>

class User : public entity_base {
    ENTITY_TABLE("users")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique())
    ENTITY_FIELD(std::string, email, not_null() | unique())
    ENTITY_FIELD(bool, is_active, default_value(true))
    ENTITY_FIELD(std::chrono::system_clock::time_point, created_at, default_now())

    ENTITY_METADATA()
};

// Type-safe ORM operations
auto users = User::query(db)
    .where("is_active = ?", true)
    .order_by("username")
    .limit(10)
    .execute();

// Automatic schema generation
entity_manager::instance().create_tables(db);
```

[🏗️ ORM Framework Guide →](docs/FEATURES_ORM_QUERY.md#orm-framework)

### Result Types

**Migration Completed**: All internal modules now use `kcenon::common::Result<T>` from common_system.

- **All code** uses `kcenon::common::Result<T>` / `kcenon::common::VoidResult` directly
- **Deprecated aliases** (`database::result<T>`, `database::Result<T>`, `database::VoidResult`) are still available for backward compatibility but will emit deprecation warnings
- **API changes**: Use `error()` instead of `get_error()`, `is_err()` instead of `is_error()`

**API Reference**:
```cpp
// Using common::Result<T>
kcenon::common::Result<int> result = some_operation();
if (result.is_ok()) {
    int value = result.value();
}
if (result.is_err()) {
    auto error = result.error();  // Returns kcenon::common::error_info
}

// Using VoidResult for operations that don't return a value
kcenon::common::VoidResult void_result = some_void_operation();
if (void_result.is_ok()) {
    // Success
}
```

For detailed information, see `database/core/result.h`.

---

## Performance Highlights

### Benchmarks (Intel i7-9750H @ 2.6GHz, 16GB RAM, SSD)

| Metric | Performance | Measurement | Notes |
|--------|-------------|-------------|-------|
| **Simple SELECT (PostgreSQL)** | 1.2ms | Manual (PostgreSQL) | Type-safe abstraction |
| **Complex JOIN (PostgreSQL)** | 15ms | Manual (PostgreSQL) | Minimal overhead |
| **Bulk INSERT (1K rows)** | 45ms | Manual (PostgreSQL) | Near-native speed |
| **Transaction TPS** | 5,000 TPS | Manual (PostgreSQL) | PostgreSQL ACID |
| **Query Builder Overhead** | <20% | Automated (Synthetic) | vs. raw SQL |
| **SQLite SELECT** | See benchmarks | Automated (SQLite in-memory) | Real I/O through SQLite engine |
| **SQLite Batch INSERT** | See benchmarks | Automated (SQLite in-memory) | Includes SQL parsing + B-tree ops |
| **SQLite Transaction** | See benchmarks | Automated (SQLite in-memory) | BEGIN + INSERT + COMMIT cycle |

**Key Insights**:
- ⚡ **Query overhead**: Minimal (<20%) for type safety and flexibility
- 🔒 **ProxyMode**: Centralized pooling via database_server for production
- 💾 **Memory efficiency**: Lightweight client library with server-side pooling

**Reproducing Real I/O Benchmarks**:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DDATABASE_BUILD_BENCHMARKS=ON -DUSE_SQLITE=ON
cmake --build build -j
./build/benchmarks/database_benchmarks --benchmark_filter=SQLite
```

[⚡ Complete Benchmarks →](docs/BENCHMARKS.md)

---

## Quick Start

### Installation via vcpkg

```bash
vcpkg install kcenon-database-system
```

In your `CMakeLists.txt`:
```cmake
find_package(database_system CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::database_system)
```

### Prerequisites

- **Compiler**: C++20 capable (GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+)
- **CMake**: 3.20+
- **Optional**: Database libraries (PostgreSQL, SQLite, MongoDB, Redis)

### Installation

```bash
# Clone repository
git clone https://github.com/kcenon/database_system.git
cd database_system

# Option 1: Using build scripts (recommended)
./scripts/dependency.sh  # Linux/macOS
# or
scripts\dependency.bat   # Windows

./scripts/build.sh       # Build project
# or
scripts\build.bat        # Windows

# Option 2: Manual CMake build
vcpkg install libpqxx sqlite3 mongo-cxx-driver hiredis

mkdir build && cd build
cmake .. -DUSE_POSTGRESQL=ON -DUSE_SQLITE=ON
cmake --build .

# Run examples
./bin/basic_usage
./bin/postgres_advanced
```

### Basic Usage

```cpp
#include <database/database_manager.h>
#include <database/core/database_context.h>

int main() {
    // Initialize database system with dependency injection
    auto context = std::make_shared<database_context>();
    auto db = std::make_shared<database_manager>(context);

    // DirectMode - for development and testing
    db->set_mode(database_types::postgres);
    db->connect("host=localhost port=5432 dbname=mydb user=admin password=secret");

    // Execute query with type-safe query builder
    auto result = db->create_query_builder(database_types::postgres)
        .select({"id", "username", "email"})
        .from("users")
        .where("is_active", "=", database_value{true})
        .order_by("created_at", sort_order::desc)
        .limit(100)
        .execute(db.get());

    if (result) {
        for (const auto& row : *result) {
            std::cout << "User: " << std::get<std::string>(row.at("username")) << std::endl;
        }
    }

    db->disconnect();
    return 0;
}
```

### Unified Database System (Zero-Config)

```cpp
#include <database/integrated/unified_database_system.h>

using namespace database::integrated;

int main() {
    // Zero-config initialization
    unified_database_system db;

    // Connect and execute
    auto conn_result = db.connect("host=localhost dbname=mydb user=admin password=secret");
    if (!conn_result) {
        std::cerr << "Connection failed: " << conn_result.error() << std::endl;
        return 1;
    }

    auto result = db.execute("SELECT * FROM users WHERE age > $1", {25});
    if (result) {
        std::cout << "Found " << result->rows.size() << " users" << std::endl;
    }

    // Built-in health checks and metrics
    auto health = db.check_health();
    auto metrics = db.get_metrics();
    std::cout << "Health: " << (health.status == health_status::healthy ? "OK" : "Degraded") << std::endl;
    std::cout << "Total queries: " << metrics.total_queries << std::endl;

    return 0;
}
```

[🚀 More Examples →](samples/)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                  Application Layer                          │
│  (Your code using database_system, unified_database_system) │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│              Database Abstraction Layer                     │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ ORM         │  │ Query Builder│  │ ProxyMode        │   │
│  │ Framework   │  │ (SQL/NoSQL)  │  │ (Server-side     │   │
│  │             │  │              │  │  pooling)        │   │
│  └─────────────┘  └──────────────┘  └──────────────────┘   │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│             Backend Implementations                         │
│  ┌──────────┐ ┌────────┐ ┌─────────┐ ┌────────┐             │
│  │PostgreSQL│ │ SQLite │ │ MongoDB │ │ Redis  │             │
│  └──────────┘ └────────┘ └─────────┘ └────────┘             │
└─────────────────────────────────────────────────────────────┘
```

**Key Components**:
- **database_manager**: Manager with DirectMode/ProxyMode support
- **ProxyMode**: Centralized pooling via database_server middleware
- **Query Builders**: Type-safe SQL/NoSQL query construction
- **ORM Framework**: C++20 concepts-based entity system
- **Backend Adapters**: PostgreSQL, SQLite, MongoDB, Redis

[🏛️ Architecture Details →](docs/ARCHITECTURE.md)

---

## Ecosystem Integration

### Ecosystem Dependency Map

```mermaid
graph TD
    A[common_system] --> B[thread_system]
    A --> C[container_system]
    B --> D[logger_system]
    B --> E[monitoring_system]
    D --> F[database_system]
    E --> F
    F --> G[network_system]
    G --> H[pacs_system]

    style F fill:#f9f,stroke:#333,stroke-width:3px
```

> **Ecosystem reference**:
> [common_system](https://github.com/kcenon/common_system) — Tier 0: Result&lt;T&gt; and IExecutor interfaces
> [thread_system](https://github.com/kcenon/thread_system) — Tier 1: Thread pool for async operations (optional)
> [container_system](https://github.com/kcenon/container_system) — Tier 1: Data serialization (optional)
> [monitoring_system](https://github.com/kcenon/monitoring_system) — Tier 3: Performance monitoring (optional)
> [network_system](https://github.com/kcenon/network_system) — Tier 4: Transport layer (consumer)
> [pacs_system](https://github.com/kcenon/pacs_system) — Tier 5: DICOM database (consumer)

### Project Dependencies

```
┌─────────────────┐     ┌─────────────────┐
│ common_system   │ ──► │database_system  │
└─────────────────┘     └─────────────────┘
         ▲                       │
         │                       ▼
┌─────────────────┐     ┌─────────────────┐
│ thread_system   │ ──► │container_system │
└─────────────────┘     └─────────────────┘
         ▲                       │
         └───────────────────────┘
                  ▼
    ┌─────────────────────────┐
    │  monitoring_system      │
    └─────────────────────────┘
```

**Related Projects**:
- **[common_system](https://github.com/kcenon/common_system)**: Common interfaces and Result<T> pattern
- **[thread_system](https://github.com/kcenon/thread_system)**: High-performance concurrent execution (connection pool v3)
- **[container_system](https://github.com/kcenon/container_system)**: Data serialization for BLOB storage
- **[monitoring_system](https://github.com/kcenon/monitoring_system)**: Performance monitoring and metrics

[🌐 Ecosystem Integration Guide →](docs/ECOSYSTEM.md)

---

## Documentation

### Getting Started
- 📖 [Getting Started Guide](docs/README.md)
- 🔧 [Build Guide](docs/guides/BUILD_GUIDE.md)
- 🚀 [Quick Start Examples](samples/)

### Core Documentation
- 📚 [Detailed Features](docs/FEATURES.md) - Backend details, ORM, query builders
- ⚡ [Performance Benchmarks](docs/BENCHMARKS.md) - Comprehensive performance data
- 🏗️ [Project Structure](docs/PROJECT_STRUCTURE.md) - Module organization, build system
- ✅ [Production Quality](docs/PRODUCTION_QUALITY.md) - Enterprise features, CI/CD, thread safety

### Advanced Topics
- 🏛️ [Architecture](docs/ARCHITECTURE.md) - System design and patterns
- 📘 [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- 🔐 [Security Guide](SECURITY.md) - Security policy and reporting <!-- TODO: dedicated docs/advanced/SECURITY.md for TLS/SSL, RBAC, audit logging -->
- 🔄 [Migration Guide](docs/advanced/MIGRATION.md) - Upgrading from previous versions

### Development
- 🤝 [Contributing](docs/contributing/CONTRIBUTING.md)
- 📋 [FAQ](docs/guides/FAQ.md)
- 🔍 [Troubleshooting](docs/guides/TROUBLESHOOTING.md)

**Build API Documentation**:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target docs
# Open documents/html/index.html
```

---

## CMake Integration

### As a Subdirectory

```cmake
add_subdirectory(database_system)
target_link_libraries(your_target PRIVATE database_system::database)
```

### With FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    database_system
    GIT_REPOSITORY https://github.com/kcenon/database_system.git
    GIT_TAG v0.1.0  # Pin to a specific release tag; do NOT use main
)
FetchContent_MakeAvailable(database_system)

target_link_libraries(your_target PRIVATE database_system::database)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_POSTGRESQL` | ON | Enable PostgreSQL support |
| `USE_SQLITE` | OFF | Enable SQLite support |
| `USE_MONGODB` | OFF | Enable MongoDB support |
| `USE_REDIS` | OFF | Enable Redis support |
| `BUILD_DATABASE_SAMPLES` | ON | Build sample programs |
| `USE_UNIT_TEST` | ON | Build unit tests |
| `BUILD_WITH_COMMON_SYSTEM` | OFF | Enable common_system integration (Result<T>, sets KCENON_HAS_COMMON_SYSTEM) |

[📦 Complete Build Guide →](docs/guides/BUILD_GUIDE.md)

---

## Production Quality

### Build & Testing Infrastructure

- ✅ **Multi-Platform CI/CD**: Ubuntu, Windows, macOS (GCC, Clang, MSVC)
- ✅ **Sanitizer Coverage**: ThreadSanitizer, AddressSanitizer, UBSanitizer (all clean)
- ✅ **Code Coverage**: 87.5% lines, 92.3% functions ([codecov](https://codecov.io/gh/kcenon/database_system))
- ✅ **Static Analysis**: Clang-tidy, Cppcheck (zero issues)

### Thread Safety & Concurrency

- ✅ **Grade A+**: ThreadSanitizer clean, zero data races
- ✅ **Lock-based coordination** for shared state
- ✅ **Atomic operations** for statistics
- ✅ **ProxyMode**: Server-side pooling for high-concurrency scenarios

### Resource Management (RAII)

- ✅ **Grade A**: 100% smart pointer usage
- ✅ **Zero memory leaks**: AddressSanitizer and Valgrind verified
- ✅ **Automatic cleanup**: All resources RAII-managed
- ✅ **Exception safety**: Strong exception safety guarantees

### Error Handling

- ✅ **Result<T> Adapters**: Type-safe error handling for external API
- ✅ **Error Codes**: -500 to -599 (centralized in common_system)
- ✅ **Transaction Safety**: Full ACID support with comprehensive error reporting

[✅ Complete Production Quality Report →](docs/PRODUCTION_QUALITY.md)

---

## Performance Baselines

**See [docs/performance/BASELINE.md](docs/performance/BASELINE.md) for detailed baseline metrics**

### Key Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Transaction TPS (PostgreSQL) | 5,000 TPS | ACID compliant |
| Simple SELECT (PostgreSQL) | 1.2ms | Minimal overhead |
| Complex JOIN (PostgreSQL) | 15ms | Type-safe abstraction |
| Bulk INSERT (1K rows) | 45ms | Near-native speed |
| Query Builder Overhead | <20% | vs. raw SQL |
| Memory Baseline | <50MB | Lightweight client |

> **Note**: Connection pooling metrics are now server-side with ProxyMode via database_server.

---

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

[🤝 Contributing Guidelines →](docs/contributing/CONTRIBUTING.md)

---

## License

BSD 3-Clause License - see [LICENSE](LICENSE) file for details.

---

## Support & Community

- 💬 [GitHub Discussions](https://github.com/kcenon/database_system/discussions)
- 🐛 [Issue Tracker](https://github.com/kcenon/database_system/issues)
- 📧 Contact: kcenon@naver.com

---

## Acknowledgments

- Inspired by modern database abstraction patterns and best practices
- Built with C++20 features (GCC 13+, Clang 17+, MSVC 2022+) for maximum performance and safety
- Maintained by kcenon@naver.com

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
