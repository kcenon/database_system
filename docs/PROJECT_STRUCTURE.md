# Database System Project Structure

**Last Updated**: 2025-11-15
**Version**: 0.3.0.0

This document provides a comprehensive guide to the database_system directory organization, module descriptions, and build configuration.

---

## Table of Contents

- [Directory Overview](#directory-overview)
- [Module Organization](#module-organization)
- [File Descriptions](#file-descriptions)
- [Build System](#build-system)
- [Dependencies](#dependencies)
- [Integration Points](#integration-points)

---

## Directory Overview

```
database_system/
├── include/database/              # Public headers
│   ├── core/                      # Core abstractions
│   ├── backends/                  # Database backend implementations
│   ├── query/                     # Query building and execution
│   ├── orm/                       # Object-Relational Mapping
│   ├── security/                  # Enterprise security
│   ├── monitoring/                # Performance monitoring
│   ├── async/                     # Asynchronous operations
│   ├── resilient/                 # Resilient connections
│   ├── integrated/                # Unified database system
│   └── adapters/                  # System adapters
├── src/                           # Implementation files
│   ├── core/                      # Core implementations
│   ├── backends/                  # Backend implementations
│   ├── query/                     # Query implementations
│   ├── orm/                       # ORM implementations
│   ├── security/                  # Security implementations
│   ├── monitoring/                # Monitoring implementations
│   ├── async/                     # Async implementations
│   ├── resilient/                 # Resilient implementations
│   ├── integrated/                # Unified system implementations
│   └── adapters/                  # Adapter implementations
├── samples/                       # Example programs
│   ├── basic_usage/               # Basic operations
│   ├── postgres_advanced/         # Advanced PostgreSQL
│   ├── connection_pool_demo/      # Connection pooling
│   ├── orm_examples/              # ORM usage
│   ├── enterprise_features/       # Security & monitoring
│   ├── query_examples/            # Query builders
│   ├── multi_database/            # Multi-backend usage
│   ├── async_examples/            # Async operations
│   ├── integrated/                # Unified system examples
│   ├── container_integration/     # Container system integration
│   ├── messaging_integration/     # Messaging integration
│   └── monitoring_integration/    # Monitoring integration
├── tests/                         # All tests
│   ├── unit/                      # Unit tests
│   │   ├── core/                  # Core tests
│   │   ├── backends/              # Backend tests
│   │   ├── query/                 # Query tests
│   │   ├── orm/                   # ORM tests
│   │   ├── security/              # Security tests
│   │   ├── monitoring/            # Monitoring tests
│   │   ├── async/                 # Async tests
│   │   ├── remote/                # Remote tests
│   │   ├── resilient/             # Resilient tests
│   │   ├── integrated/            # Unified system tests
│   │   └── adapters/              # Adapter tests
│   ├── integration/               # Integration tests
│   │   ├── postgres/              # PostgreSQL integration
│   │   ├── sqlite/                # SQLite integration
│   │   ├── mongodb/               # MongoDB integration
│   │   ├── redis/                 # Redis integration
│   │   └── multi_backend/         # Multi-backend tests
│   └── performance/               # Performance tests
│       ├── connection_pool/       # Pool benchmarks
│       ├── query_performance/     # Query benchmarks
│       ├── concurrent/            # Concurrency tests
│       └── memory/                # Memory profiling
├── benchmarks/                    # Performance benchmarks
│   ├── scripts/                   # Benchmark scripts
│   ├── results/                   # Benchmark results
│   └── BASELINE.md                # Baseline metrics
├── docs/                          # Documentation
│   ├── README.md                  # Documentation index
│   ├── FEATURES.md                # Detailed features
│   ├── BENCHMARKS.md              # Performance benchmarks
│   ├── PROJECT_STRUCTURE.md       # This file
│   ├── PRODUCTION_QUALITY.md      # Production quality
│   ├── 01-ARCHITECTURE.md         # Architecture overview
│   ├── 02-API_REFERENCE.md        # API documentation
│   ├── advanced/                  # Advanced guides
│   ├── guides/                    # User guides
│   ├── contributing/              # Contribution guides
│   ├── integration/               # Integration guides
│   └── performance/               # Performance docs
├── cmake/                         # CMake modules
│   ├── FindPostgreSQL.cmake       # PostgreSQL finder
│   ├── FindSQLite3.cmake          # SQLite3 finder
│   ├── FindMongoDB.cmake          # MongoDB finder
│   ├── FindRedis.cmake            # Redis finder
│   └── CompilerWarnings.cmake     # Compiler settings
├── scripts/                       # Build and utility scripts
│   ├── dependency.sh              # Dependency installation (Linux/macOS)
│   ├── dependency.bat             # Dependency installation (Windows)
│   ├── dependency.ps1             # Dependency installation (PowerShell)
│   ├── build.sh                   # Build script (Linux/macOS)
│   ├── build.bat                  # Build script (Windows)
│   ├── build.ps1                  # Build script (PowerShell)
│   └── run_tests.sh               # Test runner
├── .github/                       # GitHub configuration
│   └── workflows/                 # CI/CD workflows
│       ├── ci.yml                 # Main CI pipeline
│       ├── coverage.yml           # Coverage reporting
│       ├── static-analysis.yml    # Static analysis
│       └── build-Doxygen.yaml     # Documentation build
├── CMakeLists.txt                 # Main CMake configuration
├── vcpkg.json                     # vcpkg dependencies
├── LICENSE                        # BSD 3-Clause License
└── README.md                      # Main README (simplified)
```

---

## Module Organization

### Core Module (`include/database/core/`, `src/core/`)

**Purpose**: Foundation abstractions and interfaces for all database operations

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `database_base.h` | Abstract interface for database backends | 250 |
| `database_manager.h` | Singleton manager with pooling | 350 |
| `database_types.h` | Type definitions and enums | 180 |
| `connection_pool.h` | Enterprise connection pooling | 450 |
| `connection_wrapper.h` | RAII connection wrapper | 120 |
| `database_exceptions.h` | Exception hierarchy | 80 |

**Responsibilities**:
- Define abstract `database_base` interface
- Manage database backend lifecycle
- Provide connection pooling infrastructure
- Define data types (`database_value`, `database_row`, `database_result`)
- Handle database-agnostic operations

**Dependencies**:
- C++ Standard Library
- Optional: thread_system (for connection pool v3)
- Optional: logger_system (for logging)

### Backend Module (`include/database/backends/`, `src/backends/`)

**Purpose**: Concrete implementations for each database backend

#### PostgreSQL Backend

**Files**:
- `postgres_manager.h/cpp`: PostgreSQL implementation (850 LOC)
- `postgres_connection.h/cpp`: Connection handling (320 LOC)
- `postgres_prepared_statement.h/cpp`: Prepared statements (280 LOC)

**Features**:
- JSONB support
- Array types
- CTEs (Common Table Expressions)
- Prepared statements
- Full-text search

**Dependencies**:
- libpqxx (PostgreSQL C++ client library)
- OpenSSL (for TLS/SSL)

#### SQLite Backend

**Files**:
- `sqlite/sqlite_manager.h/cpp`: SQLite implementation (620 LOC)
- `sqlite/sqlite_connection.h/cpp`: Connection handling (240 LOC)

**Features**:
- WAL mode
- FTS5 full-text search
- In-memory databases
- JSON1 extension

**Dependencies**:
- sqlite3 (embedded database library)

#### MongoDB Backend

**Files**:
- `mongodb/mongodb_manager.h/cpp`: MongoDB implementation (920 LOC)
- `mongodb/mongodb_connection.h/cpp`: Connection handling (380 LOC)
- `mongodb/gridfs_handler.h/cpp`: GridFS support (280 LOC)

**Features**:
- Document operations
- Aggregation pipeline
- GridFS for large files
- Change streams

**Dependencies**:
- mongo-cxx-driver (MongoDB C++ driver)
- libbson (BSON library)

#### Redis Backend

**Files**:
- `redis/redis_manager.h/cpp`: Redis implementation (680 LOC)
- `redis/redis_connection.h/cpp`: Connection handling (260 LOC)
- `redis/redis_pubsub.h/cpp`: Pub/Sub support (220 LOC)

**Features**:
- All data types (String, Hash, List, Set, Sorted Set)
- Pub/Sub messaging
- Transactions (MULTI/EXEC)
- Lua scripting
- Pipelining

**Dependencies**:
- hiredis (Redis C client library)
- OpenSSL (for TLS/SSL)

### Query Module (`include/database/query/`, `src/query/`)

**Purpose**: Type-safe query construction for SQL and NoSQL

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `query_builder.h` | Abstract query builder interface | 200 |
| `sql_builder.h` | SQL query builder | 650 |
| `nosql_builder.h` | NoSQL query builder | 480 |
| `immutable_query_builder.h` | Thread-safe immutable builder | 550 |
| `prepared_statement.h` | Prepared statement support | 280 |

**Features**:
- Fluent API for query construction
- Compile-time type safety
- SQL and NoSQL support
- Immutable builder (thread-safe)
- Prepared statement integration

**Dependencies**:
- Core module

### ORM Module (`include/database/orm/`, `src/orm/`)

**Purpose**: Object-Relational Mapping with C++17 SFINAE

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `entity.h` | Entity base class and macros | 480 |
| `entity_manager.h` | Entity lifecycle management | 520 |
| `schema_manager.h` | Schema generation/migration | 620 |
| `relationship.h` | Entity relationships | 380 |
| `query_builder_orm.h` | ORM query builder | 450 |

**Features**:
- C++17 SFINAE-based entity definition
- Automatic schema generation
- Type-safe entity operations (CRUD)
- Relationships (one-to-many, many-to-one, many-to-many)
- Migrations and versioning

**Dependencies**:
- Core module
- Query module

### Security Module (`include/database/security/`, `src/security/`)

**Purpose**: Enterprise security features

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `secure_connection.h` | TLS/SSL connection management | 380 |
| `credential_manager.h` | Secure credential storage | 320 |
| `access_control.h` | Role-based access control | 450 |
| `audit_logger.h` | Security audit logging | 280 |
| `encryption.h` | Encryption utilities | 220 |

**Features**:
- TLS/SSL encryption
- Secure credential hashing (bcrypt, argon2)
- Role-based access control (RBAC)
- Audit logging
- Certificate verification

**Dependencies**:
- Core module
- OpenSSL
- Optional: logger_system

### Monitoring Module (`include/database/monitoring/`, `src/monitoring/`)

**Purpose**: Performance monitoring and observability

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `performance_monitor.h` | Real-time metrics | 420 |
| `health_monitor.h` | Database health checks | 320 |
| `prometheus_exporter.h` | Prometheus integration | 380 |
| `alert_manager.h` | Performance alerting | 280 |

**Features**:
- Real-time performance metrics
- Query latency tracking (P50, P95, P99)
- Connection pool monitoring
- Prometheus export
- Alert thresholds

**Dependencies**:
- Core module
- Optional: monitoring_system (for enhanced metrics)

### Async Module (`include/database/async/`, `src/async/`)

**Purpose**: Asynchronous database operations

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `async_operations.h` | C++20 coroutine support | 520 |
| `future_operations.h` | C++17 future-based async | 380 |
| `stream_processor.h` | Real-time streaming | 450 |

**Features**:
- C++20 coroutines (optional)
- C++17 std::future fallback
- Real-time data streaming
- Async connection pooling

**Dependencies**:
- Core module
- Optional: thread_system (for async executor)

### Resilient Module (`include/database/resilient/`, `src/resilient/`)

**Purpose**: Production-grade reliability

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `resilient_connection.h` | Auto-reconnect | 480 |
| `health_scoring.h` | Health monitoring | 320 |
| `circuit_breaker.h` | Circuit breaker pattern | 280 |
| `retry_policy.h` | Retry strategies | 220 |

**Features**:
- Automatic reconnection
- Exponential backoff
- Health scoring
- Circuit breaker
- Retry policies

**Dependencies**:
- Core module

### Integrated Module (`include/database/integrated/`, `src/integrated/`)

**Purpose**: Zero-config unified database system

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `unified_database_system.h` | Unified interface | 680 |
| `database_logger_adapter.h` | Logger adapter | 280 |
| `database_monitor_adapter.h` | Monitor adapter | 320 |
| `database_thread_adapter.h` | Thread pool adapter | 350 |

**Features**:
- Zero-configuration usage
- Integrated logging (logger_system)
- Integrated monitoring (monitoring_system)
- Integrated threading (thread_system)
- Builder pattern configuration
- Fallback implementations

**Dependencies**:
- Core module
- Optional: logger_system, monitoring_system, thread_system

### Adapters Module (`include/database/adapters/`, `src/adapters/`)

**Purpose**: Ecosystem integration with Result<T> pattern

**Key Files**:

| File | Description | Lines of Code |
|------|-------------|---------------|
| `common_system_adapter.h` | Result<T> adapter | 520 |
| `common_connection_pool_adapter.h` | Pool adapter | 380 |
| `common_database_factory.h` | Factory pattern | 280 |

**Features**:
- Result<T> error handling
- Transaction support with Result<T>
- Connection pool integration
- Factory pattern for database creation

**Dependencies**:
- Core module
- common_system (for Result<T>)

---

## File Descriptions

### Core Files

#### `database_base.h/cpp`

**Purpose**: Abstract interface for all database backends

**Key Methods**:
```cpp
class database_base {
public:
    virtual ~database_base() = default;

    // Connection management
    virtual bool connect(const std::string& connection_string) = 0;
    virtual bool disconnect() = 0;
    virtual bool is_connected() const = 0;

    // Query execution
    virtual database_result select_query(const std::string& query) = 0;
    virtual unsigned int insert_query(const std::string& query) = 0;
    virtual unsigned int update_query(const std::string& query) = 0;
    virtual unsigned int delete_query(const std::string& query) = 0;

    // Transaction support
    virtual bool begin_transaction() = 0;
    virtual bool commit() = 0;
    virtual bool rollback() = 0;

    // Prepared statements
    virtual std::shared_ptr<prepared_statement> prepare(const std::string& query) = 0;
};
```

**Inheritance Hierarchy**:
```
database_base (abstract)
├── postgres_manager
├── sqlite_manager
├── mongodb_manager
└── redis_manager
```

#### `database_manager.h/cpp`

**Purpose**: Singleton manager with connection pooling

**Key Methods**:
```cpp
class database_manager {
public:
    static database_manager& handle();

    // Backend management
    bool set_mode(database_types type);
    database_types get_mode() const;

    // Connection pooling
    bool create_connection_pool(database_types type, const connection_pool_config& config);
    std::shared_ptr<connection_pool> get_connection_pool(database_types type);
    void destroy_connection_pool(database_types type);

    // Query builders
    query_builder create_query_builder(database_types type);

    // Statistics
    std::map<database_types, pool_statistics> get_pool_stats();
};
```

**Usage Pattern**:
```cpp
auto& db = database_manager::handle();
db.set_mode(database_types::postgres);
db.create_connection_pool(database_types::postgres, config);
```

#### `connection_pool.h/cpp`

**Purpose**: Enterprise-grade connection pooling

**Key Classes**:
```cpp
struct connection_pool_config {
    size_t min_connections = 2;
    size_t max_connections = 20;
    std::chrono::milliseconds acquire_timeout{5000};
    std::chrono::milliseconds idle_timeout{30000};
    std::chrono::milliseconds health_check_interval{60000};
    bool enable_health_checks = true;
    std::string connection_string;
};

class connection_pool {
public:
    // Connection acquisition (RAII)
    std::shared_ptr<connection_wrapper> acquire_connection(
        connection_priority priority = connection_priority::normal
    );

    // Pool management
    void resize(size_t new_size);
    void shutdown();

    // Statistics
    pool_statistics get_statistics() const;
    bool is_healthy() const;
};
```

---

## Build System

### CMake Configuration

**Main `CMakeLists.txt`**:

```cmake
cmake_minimum_required(VERSION 3.16)
project(database_system VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(USE_POSTGRESQL "Enable PostgreSQL support" ON)
option(USE_SQLITE "Enable SQLite support" OFF)
option(USE_MONGODB "Enable MongoDB support" OFF)
option(USE_REDIS "Enable Redis support" OFF)
option(BUILD_DATABASE_SAMPLES "Build sample programs" ON)
option(USE_UNIT_TEST "Build unit tests" ON)
option(BUILD_WITH_COMMON_SYSTEM "Build with common_system integration" OFF)

# Find dependencies
if(USE_POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
endif()

if(USE_SQLITE)
    find_package(SQLite3 REQUIRED)
endif()

if(USE_MONGODB)
    find_package(MongoDB REQUIRED)
endif()

if(USE_REDIS)
    find_package(Redis REQUIRED)
endif()

# Main library
add_library(database_system
    src/core/database_manager.cpp
    src/core/connection_pool.cpp
    # ... more sources
)

target_include_directories(database_system
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
if(USE_POSTGRESQL)
    target_link_libraries(database_system PUBLIC PostgreSQL::PostgreSQL)
    target_compile_definitions(database_system PUBLIC USE_POSTGRESQL)
endif()

# Samples
if(BUILD_DATABASE_SAMPLES)
    add_subdirectory(samples)
endif()

# Tests
if(USE_UNIT_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### Build Targets

| Target | Description | Command |
|--------|-------------|---------|
| `database_system` | Main library | `cmake --build build --target database_system` |
| `basic_usage` | Basic example | `cmake --build build --target basic_usage` |
| `postgres_advanced` | PostgreSQL example | `cmake --build build --target postgres_advanced` |
| `connection_pool_demo` | Pool demo | `cmake --build build --target connection_pool_demo` |
| `database_test` | All unit tests | `cmake --build build --target database_test` |
| `docs` | Doxygen docs | `cmake --build build --target docs` |

### CMake Presets

**`CMakePresets.json`**:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "default",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "USE_POSTGRESQL": "ON",
        "USE_SQLITE": "ON",
        "BUILD_DATABASE_SAMPLES": "ON"
      }
    },
    {
      "name": "debug",
      "inherits": "default",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "USE_UNIT_TEST": "ON"
      }
    },
    {
      "name": "all-backends",
      "inherits": "default",
      "cacheVariables": {
        "USE_POSTGRESQL": "ON",
        "USE_SQLITE": "ON",
        "USE_MONGODB": "ON",
        "USE_REDIS": "ON"
      }
    }
  ]
}
```

---

## Dependencies

### Required Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| CMake | 3.16+ | Build system |
| C++ Compiler | C++17 | GCC 7+, Clang 5+, MSVC 2017+ |

### Optional Database Dependencies

| Database | Library | Version | vcpkg Package |
|----------|---------|---------|---------------|
| PostgreSQL | libpqxx | 7.7+ | `libpqxx` |
| SQLite | sqlite3 | 3.40+ | `sqlite3` |
| MongoDB | mongo-cxx-driver | 3.7+ | `mongo-cxx-driver` |
| Redis | hiredis | 1.1+ | `hiredis` |

### Optional System Dependencies

| System | Purpose | vcpkg Package |
|--------|---------|---------------|
| thread_system | Connection pool v3, async | `kcenon-thread-system` |
| logger_system | Structured logging | `kcenon-logger-system` |
| monitoring_system | Enhanced metrics | `kcenon-monitoring-system` |
| common_system | Result<T> pattern | `kcenon-common-system` |
| container_system | Serialization | `kcenon-container-system` |

### Installation

**Using vcpkg**:
```bash
# Install database libraries
vcpkg install libpqxx openssl sqlite3 mongo-cxx-driver hiredis

# Install optional systems (if available)
vcpkg install kcenon-common-system kcenon-thread-system
```

**Using build scripts**:
```bash
# Linux/macOS
./scripts/dependency.sh

# Windows (Command Prompt)
scripts\dependency.bat

# Windows (PowerShell)
.\scripts\dependency.ps1
```

---

## Integration Points

### Common System Integration

**Result<T> Error Handling**:

```cpp
#include <database/adapters/common_system_adapter.h>

auto db = std::make_shared<postgres_manager>();
auto adapter = std::make_shared<common_system_database_adapter>(db);

auto result = adapter->connect("host=localhost dbname=mydb");
if (!result) {
    std::cerr << "Error: " << result.error().message << std::endl;
    return -1;
}
```

### Thread System Integration

**Connection Pool v3 with Thread Pool**:

```cpp
#include <thread_system/thread_pool.h>
#include <database/connection_pool.h>

auto thread_pool = std::make_shared<thread_system::thread_pool>(8);

connection_pool_config config;
config.thread_pool = thread_pool;  // Enable thread_system integration
config.min_connections = 10;
config.max_connections = 100;

db.create_connection_pool(database_types::postgres, config);
```

### Logger System Integration

**Structured Logging**:

```cpp
#include <logger_system/logger.h>
#include <database/integrated/unified_database_system.h>

auto logger = logger_system::createLogger("database.log");

auto db = unified_database_system::builder()
    .with_logger(logger)
    .build();

// All database operations automatically logged
```

### Monitoring System Integration

**Performance Metrics**:

```cpp
#include <monitoring_system/prometheus_exporter.h>
#include <database/monitoring/performance_monitor.h>

auto& monitor = performance_monitor::instance();
monitor.set_monitoring_system(monitoring_system::instance());

// Metrics automatically exported to Prometheus on port 9090
```

### Container System Integration

**Serialized Storage**:

```cpp
#include <container_system/variant_value.h>
#include <database/database_manager.h>

container_system::variant_value data;
data["user_id"] = 12345;
data["settings"] = {{"theme", "dark"}, {"language", "en"}};

auto serialized = data.to_msgpack();

db.insert_query(
    "INSERT INTO user_data (user_id, data) VALUES (?, ?)",
    {12345, serialized}
);
```

---

## Build Artifacts

### Library Output

| Configuration | Output | Location |
|--------------|--------|----------|
| Static Library | `libdatabase_system.a` | `build/lib/` |
| Shared Library | `libdatabase_system.so` | `build/lib/` |
| Windows DLL | `database_system.dll` | `build/bin/` |

### Sample Programs

| Program | Binary | Location |
|---------|--------|----------|
| Basic Usage | `basic_usage` | `build/bin/samples/` |
| PostgreSQL Advanced | `postgres_advanced` | `build/bin/samples/` |
| Connection Pool Demo | `connection_pool_demo` | `build/bin/samples/` |
| ORM Examples | `orm_examples` | `build/bin/samples/` |
| Unified System | `unified_basic_usage` | `build/bin/samples/integrated/` |

### Test Executables

| Test Suite | Binary | Location |
|------------|--------|----------|
| Core Tests | `database_core_test` | `build/bin/tests/unit/` |
| Backend Tests | `database_backends_test` | `build/bin/tests/unit/` |
| Query Tests | `database_query_test` | `build/bin/tests/unit/` |
| Integration Tests | `database_integration_test` | `build/bin/tests/integration/` |
| Performance Tests | `database_performance_test` | `build/bin/tests/performance/` |

---

## Maintenance

### Code Organization Best Practices

1. **One class per file**: Each class has separate `.h` and `.cpp`
2. **Namespace organization**: All code in `database` namespace
3. **Include guards**: Use `#pragma once` for all headers
4. **Forward declarations**: Minimize header dependencies
5. **PIMPL idiom**: Hide implementation details where appropriate

### Adding New Backends

**Steps**:
1. Create `include/database/backends/<backend>/` directory
2. Implement `database_base` interface in `<backend>_manager.h/cpp`
3. Add CMake option `USE_<BACKEND>`
4. Add to `database_types` enum
5. Update `database_manager` factory
6. Add tests in `tests/unit/backends/<backend>/`
7. Add integration tests in `tests/integration/<backend>/`
8. Update documentation

**Template**:
```cpp
// include/database/backends/newdb/newdb_manager.h
#pragma once
#include "database/core/database_base.h"

namespace database {

class newdb_manager : public database_base {
public:
    newdb_manager();
    ~newdb_manager() override;

    // Implement database_base interface
    bool connect(const std::string& connection_string) override;
    // ... other methods
};

} // namespace database
```

---

**For detailed feature documentation**, see [FEATURES.md](FEATURES.md)
**For performance benchmarks**, see [BENCHMARKS.md](BENCHMARKS.md)
**For production quality details**, see [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md)

---

**Last Updated**: 2025-11-15
**Maintained by**: kcenon@naver.com
