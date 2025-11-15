[![CI](https://github.com/kcenon/database_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/database_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/database_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/database_system/actions/workflows/static-analysis.yml)
[![codecov](https://codecov.io/gh/kcenon/database_system/branch/main/graph/badge.svg)](https://codecov.io/gh/kcenon/database_system)

# Database System

> **Language:** **English** | [한국어](README_KO.md)

---

## Overview

A production-ready, enterprise-grade C++17/C++20 database abstraction layer providing unified access to multiple database backends with advanced features including ORM framework, real-time performance monitoring, enterprise security, and asynchronous operations.

**Key Value Proposition**: Eliminate vendor lock-in, maximize performance, and accelerate development with a comprehensive database solution that supports PostgreSQL, MySQL, SQLite, MongoDB, and Redis through a unified, type-safe interface.

### Latest Updates (2025-11)

- **monitoring_system Integration**: Full integration for production-grade metrics collection
- **Connection Pool v3**: 65x latency improvement (5μs → 77ns) with thread_system integration
- **Remote Database Access**: Database Proxy Server and Remote Client for distributed operations
- **Resilient Connections**: Automatic reconnection with health monitoring (<1s recovery)
- **Immutable Query Builder**: Thread-safe query construction with functional programming style
- All CI/CD pipelines green across platforms

---

## Core Features

### Multi-Backend Support

| Database | Status | Key Features | Performance |
|----------|--------|--------------|-------------|
| **PostgreSQL** | ✅ Full | JSONB, Arrays, CTEs, FTS | 1.2ms SELECT, 5K TPS |
| **MySQL** | ✅ Full | Full-text search, Transactions | 1.5ms SELECT, 4.2K TPS |
| **SQLite** | ✅ Full | WAL mode, FTS5, In-memory | 0.8ms SELECT |
| **MongoDB** | ✅ Full | Documents, Aggregation, GridFS | 2.1ms insertOne |
| **Redis** | ✅ Full | All data types, Pub/Sub, Lua | 0.3ms GET/SET |

[📚 Detailed Backend Features →](docs/FEATURES.md)

### Enterprise-Grade Connection Pooling

**Connection Pool v3 Performance**:
- **77ns** connection acquisition latency (65x faster than v2)
- **1.16M+ ops/s** throughput with thread_system integration
- **10,000+ concurrent connections** supported
- **95%+ pool efficiency** maintained under load
- Priority-based connection scheduling
- Automatic health monitoring and recovery

```cpp
#include <database/connection_pool.h>

connection_pool_config config;
config.min_connections = 10;
config.max_connections = 100;
config.connection_string = "host=localhost port=5432 dbname=mydb";

auto& db = database_manager::handle();
db.create_connection_pool(database_types::postgres, config);

// RAII-managed connection (automatically returned to pool)
auto pool = db.get_connection_pool(database_types::postgres);
auto connection = pool->acquire_connection();
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

[📘 Complete Query Builder Guide →](docs/FEATURES.md#query-builders)

### ORM Framework (C++17 SFINAE-based)

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

[🏗️ ORM Framework Guide →](docs/FEATURES.md#orm-framework)

---

## Performance Highlights

### Benchmarks (Intel i7-9750H @ 2.6GHz, 16GB RAM, SSD)

| Metric | Performance | vs. Native | Notes |
|--------|-------------|-----------|-------|
| **Connection Acquisition** | 0.1ms | 20x faster | Pooled vs. native |
| **Connection Pool v3** | 77ns | 65x faster | vs. v2 (5μs) |
| **Throughput** | 1.16M+ ops/s | 7.7x faster | High load scenario |
| **Simple SELECT (PostgreSQL)** | 1.2ms | +20% overhead | Type-safe abstraction |
| **Complex JOIN (PostgreSQL)** | 15ms | +7% overhead | Minimal impact |
| **Bulk INSERT (1K rows)** | 45ms | +7% overhead | Near-native speed |
| **Transaction TPS** | 5,000 TPS | +19% faster | PostgreSQL |
| **Concurrent Connections** | 10,000+ | Stable | 95%+ efficiency |

**Key Insights**:
- 🚀 **Connection pooling**: 20x faster than native drivers
- ⚡ **Query overhead**: Minimal (<20%) for type safety and flexibility
- 📈 **Scalability**: Linear scaling up to 10,000+ concurrent connections
- 💾 **Memory efficiency**: <50MB baseline, 850MB at 10K connections

[⚡ Complete Benchmarks →](docs/BENCHMARKS.md)

---

## Quick Start

### Prerequisites

- **Compiler**: C++17 capable (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake**: 3.16+
- **Optional**: Database libraries (PostgreSQL, MySQL, SQLite, MongoDB, Redis)

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
vcpkg install libpqxx libmysql sqlite3 mongo-cxx-driver hiredis

mkdir build && cd build
cmake .. -DUSE_POSTGRESQL=ON -DUSE_SQLITE=ON
cmake --build .

# Run examples
./bin/basic_usage
./bin/connection_pool_demo
```

### Basic Usage

```cpp
#include <database/database_manager.h>
#include <database/connection_pool.h>

int main() {
    // Initialize database system
    database_manager& db = database_manager::handle();

    // Configure connection pool
    connection_pool_config config;
    config.min_connections = 10;
    config.max_connections = 100;
    config.connection_string = "host=localhost port=5432 dbname=mydb user=admin password=secret";

    db.set_mode(database_types::postgres);
    db.create_connection_pool(database_types::postgres, config);

    // Execute query with type-safe query builder
    auto result = db.create_query_builder(database_types::postgres)
        .select({"id", "username", "email"})
        .from("users")
        .where("is_active", "=", database_value{true})
        .order_by("created_at", sort_order::desc)
        .limit(100)
        .execute(&db);

    if (result) {
        for (const auto& row : *result) {
            std::cout << "User: " << std::get<std::string>(row.at("username")) << std::endl;
        }
    }

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
│  │ ORM         │  │ Query Builder│  │ Connection Pool  │   │
│  │ Framework   │  │ (SQL/NoSQL)  │  │ (v3: 77ns, 1.16M │   │
│  │             │  │              │  │  ops/s)          │   │
│  └─────────────┘  └──────────────┘  └──────────────────┘   │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│             Backend Implementations                         │
│  ┌──────────┐ ┌──────┐ ┌────────┐ ┌─────────┐ ┌────────┐  │
│  │PostgreSQL│ │MySQL │ │ SQLite │ │ MongoDB │ │ Redis  │  │
│  └──────────┘ └──────┘ └────────┘ └─────────┘ └────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**Key Components**:
- **database_manager**: Singleton manager with connection pooling
- **connection_pool v3**: Enterprise-grade pooling (77ns, 1.16M+ ops/s)
- **Query Builders**: Type-safe SQL/NoSQL query construction
- **ORM Framework**: C++17 SFINAE-based entity system
- **Backend Adapters**: PostgreSQL, MySQL, SQLite, MongoDB, Redis

[🏛️ Architecture Details →](docs/01-ARCHITECTURE.md)

---

## Ecosystem Integration

### Project Dependencies

```
┌─────────────────┐     ┌─────────────────┐
│container_system │ ──► │database_system  │
└─────────────────┘     └─────────┬───────┘
         │                        │ provides storage for
         │                        ▼
┌─────────▼───────┐     ┌─────────────────┐
│messaging_system │ ◄──► │ network_system  │
└─────────────────┘     └─────────────────┘
         │                        │
         └────────┬───────────────┘
                  ▼
    ┌─────────────────────────┐
    │  monitoring_system      │
    └─────────────────────────┘
```

**Related Projects**:
- **[container_system](https://github.com/kcenon/container_system)**: Data serialization for BLOB storage
- **[thread_system](https://github.com/kcenon/thread_system)**: High-performance concurrent execution (connection pool v3)
- **[messaging_system](https://github.com/kcenon/messaging_system)**: Message persistence and queuing
- **[network_system](https://github.com/kcenon/network_system)**: Network-based database operations
- **[monitoring_system](https://github.com/kcenon/monitoring_system)**: Performance monitoring and metrics

[🌐 Ecosystem Integration Guide →](../ECOSYSTEM_INTEGRATION.md)

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
- 🏛️ [Architecture](docs/01-ARCHITECTURE.md) - System design and patterns
- 📘 [API Reference](docs/02-API_REFERENCE.md) - Complete API documentation
- 🔐 [Security Guide](docs/advanced/SECURITY.md) - TLS/SSL, RBAC, audit logging
- 🔄 [Migration Guide](docs/guides/MIGRATION_GUIDE.md) - Upgrading from previous versions

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
target_link_libraries(your_target PRIVATE DatabaseSystem::database)
```

### With FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    database_system
    GIT_REPOSITORY https://github.com/kcenon/database_system.git
    GIT_TAG main
)
FetchContent_MakeAvailable(database_system)

target_link_libraries(your_target PRIVATE DatabaseSystem::database)
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_POSTGRESQL` | ON | Enable PostgreSQL support |
| `USE_MYSQL` | OFF | Enable MySQL support |
| `USE_SQLITE` | OFF | Enable SQLite support |
| `USE_MONGODB` | OFF | Enable MongoDB support |
| `USE_REDIS` | OFF | Enable Redis support |
| `BUILD_DATABASE_SAMPLES` | ON | Build sample programs |
| `USE_UNIT_TEST` | ON | Build unit tests |
| `BUILD_WITH_COMMON_SYSTEM` | OFF | Enable common_system integration (Result<T>) |

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
- ✅ **10,000+ concurrent connections** supported
- ✅ **95%+ pool efficiency** under high load
- ✅ **Lock-based coordination** for shared state
- ✅ **Atomic operations** for statistics

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

**See [benchmarks/BASELINE.md](benchmarks/BASELINE.md) for detailed baseline metrics**

### Key Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Connection Pool Acquisition | 0.1ms | 20x faster than native |
| Connection Pool v3 Latency | 77ns | 65x improvement vs v2 |
| Throughput (high load) | 1.16M+ ops/s | With thread_system |
| Transaction TPS (PostgreSQL) | 5,000 TPS | ACID compliant |
| Simple SELECT (PostgreSQL) | 1.2ms | Minimal overhead |
| Concurrent Connections | 10,000+ | Stable, 95%+ efficiency |
| Memory Baseline | <50MB | Efficient resource usage |

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
- Built with C++17/C++20 features for maximum performance and safety
- Maintained by kcenon@naver.com

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
