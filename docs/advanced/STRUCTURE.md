# Database System Project Structure

> **Version:** 0.1.0.0
> **Last Updated:** 2025-10-22
> **Language:** English

## Table of Contents

- [Overview](#overview)
- [Directory Tree](#directory-tree)
- [Namespace Organization](#namespace-organization)
- [Backend-Specific Implementations](#backend-specific-implementations)
- [Module Dependencies](#module-dependencies)
- [File Organization Patterns](#file-organization-patterns)

---

## Overview

The Database System follows a modular architecture with clear separation of concerns. The project is organized into distinct subdirectories within the `database/` folder, each responsible for specific functionality.

### Design Principles

1. **Separation of Concerns**: Each module handles a single responsibility
2. **Namespace Hierarchy**: Mirrors directory structure for consistency
3. **Backend Isolation**: Backend-specific code isolated in subdirectories
4. **Header-Only Core**: Core abstractions available as headers
5. **Incremental Linking**: Optional backend libraries linked as needed

---

## Directory Tree

### Root Structure

```
database_system/
├── database/                           # Main module directory
│   ├── adapters/                       # Integration adapters
│   │   └── common_system_adapter.h     # Result<T> adapter for common_system
│   ├── async/                          # Asynchronous operations
│   │   └── async_operations.h          # Async database operations, coroutines
│   ├── backends/                       # Database backend implementations
│   │   ├── mongodb/                    # MongoDB NoSQL backend
│   │   │   ├── mongodb_manager.h       # MongoDB manager interface
│   │   │   └── mongodb_manager.cpp     # MongoDB implementation
│   │   ├── mysql/                      # MySQL relational backend
│   │   │   ├── mysql_manager.h         # MySQL manager interface
│   │   │   └── mysql_manager.cpp       # MySQL implementation
│   │   ├── redis/                      # Redis key-value backend
│   │   │   ├── redis_manager.h         # Redis manager interface
│   │   │   └── redis_manager.cpp       # Redis implementation
│   │   └── sqlite/                     # SQLite embedded backend
│   │       ├── sqlite_manager.h        # SQLite manager interface
│   │       └── sqlite_manager.cpp      # SQLite implementation
│   ├── monitoring/                     # Performance monitoring
│   │   ├── performance_monitor.h       # Performance metrics collection
│   │   └── performance_monitor.cpp     # Performance monitor implementation
│   ├── orm/                            # Object-Relational Mapping framework
│   │   ├── entity.h                    # Entity base class and metadata
│   │   └── entity.cpp                  # Entity implementation
│   ├── security/                       # Enterprise security features
│   │   └── secure_connection.h         # Secure connection management
│   ├── connection_leak_detector.h      # Connection leak detection
│   ├── connection_pool.h               # Connection pooling interface
│   ├── connection_pool.cpp             # Connection pool implementation
│   ├── database_base.h                 # Abstract database interface
│   ├── database_manager.h              # Database manager interface
│   ├── database_manager.cpp            # Database manager implementation
│   ├── database_types.h                # Type definitions and enums
│   ├── leak_detector_enhanced.h        # Enhanced leak detector
│   ├── postgres_manager.h              # PostgreSQL manager (primary backend)
│   ├── postgres_manager.cpp            # PostgreSQL implementation
│   ├── query_builder.h                 # Query builder interface
│   └── query_builder.cpp               # Query builder implementation
├── benchmarks/                         # Performance benchmarks
│   ├── connection_pool_bench.cpp       # Connection pool benchmarks
│   ├── main_bench.cpp                  # Main benchmark runner
│   ├── query_execution_bench.cpp       # Query execution benchmarks
│   └── transaction_bench.cpp           # Transaction benchmarks
├── docs/                               # Documentation
│   ├── API_REFERENCE.md                # Complete API reference
│   ├── API_REFERENCE_KO.md             # API reference (Korean)
│   ├── ARCHITECTURE.md                 # System architecture (detailed)
│   ├── ARCHITECTURE_KO.md              # Architecture (Korean)
│   ├── BUILD_GUIDE.md                  # Build instructions
│   ├── BUILD_GUIDE_KO.md               # Build guide (Korean)
│   ├── CURRENT_STATE.md                # Current implementation status
│   ├── CURRENT_STATE_KO.md             # Current state (Korean)
│   ├── PERFORMANCE_BENCHMARKS.md       # Performance benchmark results
│   ├── PERFORMANCE_BENCHMARKS_KO.md    # Benchmarks (Korean)
│   ├── SAMPLES_GUIDE.md                # Sample code guide
│   └── SAMPLES_GUIDE_KO.md             # Samples guide (Korean)
├── integration_tests/                  # Integration test suite
│   ├── failures/                       # Error handling tests
│   │   └── error_handling_test.cpp     # Error scenario tests
│   ├── framework/                      # Test framework utilities
│   │   ├── system_fixture.h            # Test fixtures
│   │   └── test_helpers.h              # Helper functions
│   ├── performance/                    # Performance tests
│   │   └── database_performance_test.cpp # Performance validation
│   └── scenarios/                      # Integration scenarios
│       ├── connection_management_test.cpp # Connection pool tests
│       └── query_execution_test.cpp    # Query execution tests
├── samples/                            # Example applications
│   ├── async_operations_demo.cpp       # Async operations example
│   ├── basic_usage.cpp                 # Basic database operations
│   ├── connection_pool_demo.cpp        # Connection pooling demo
│   ├── orm_framework_demo.cpp          # ORM framework usage
│   ├── performance_monitoring_demo.cpp # Performance monitoring example
│   ├── postgres_advanced.cpp           # Advanced PostgreSQL features
│   ├── run_all_samples.cpp             # Sample runner
│   └── security_framework_demo.cpp     # Security features demo
├── tests/                              # Unit tests
│   ├── benchmark_tests.cpp             # Benchmark validation tests
│   ├── integration_tests.cpp           # Integration test suite
│   ├── thread_safety_tests.cpp         # Thread safety validation
│   └── unit_tests.cpp                  # Unit test suite
├── ARCHITECTURE.md                     # Architecture overview (root)
├── BASELINE.md                         # Performance baselines
├── BASELINE_KO.md                      # Baselines (Korean)
├── CHANGELOG.md                        # Development changelog
├── CHANGELOG_KO.md                     # Changelog (Korean)
├── CMakeLists.txt                      # Main build configuration
├── IMPLEMENTATION_SUMMARY.md           # Implementation summary
├── IMPLEMENTATION_SUMMARY_KO.md        # Summary (Korean)
├── IMPROVEMENTS.md                     # Planned improvements
├── IMPROVEMENTS_KO.md                  # Improvements (Korean)
├── INTEGRATION.md                      # Integration guide
├── MIGRATION.md                        # Migration guide (root)
├── README.md                           # Project overview
├── README_KO.md                        # README (Korean)
├── STRUCTURE.md                        # Project structure (this file)
└── vcpkg.json                          # Dependency manifest
```

### Key Directory Purposes

| Directory | Purpose | Key Files |
|-----------|---------|-----------|
| `database/` | Core database module | All core headers and implementations |
| `database/backends/` | Backend implementations | mysql, sqlite, mongodb, redis subdirectories |
| `database/orm/` | ORM framework | entity.h, entity metadata system |
| `database/monitoring/` | Performance monitoring | performance_monitor.h/cpp |
| `database/security/` | Security features | secure_connection.h, access control |
| `database/adapters/` | Integration adapters | common_system_adapter.h |
| `database/async/` | Async operations | async_operations.h, coroutines |
| `benchmarks/` | Performance tests | Connection pool, query, transaction benchmarks |
| `docs/` | Documentation | API reference, architecture, guides |
| `integration_tests/` | Integration tests | Multi-component testing scenarios |
| `samples/` | Example code | Usage demonstrations |
| `tests/` | Unit tests | Component-level testing |

---

## Namespace Organization

The namespace hierarchy mirrors the directory structure for consistency and clarity.

### Root Namespace

```cpp
namespace database {
    // Core types and abstractions
}
```

### Nested Namespaces

```cpp
namespace database {

    // Core database abstractions (database/)
    class database_base { /* ... */ };
    class database_manager { /* ... */ };
    class connection_pool { /* ... */ };
    class query_builder { /* ... */ };

    // Type definitions (database/database_types.h)
    enum class database_types { /* ... */ };
    using database_value = std::variant<...>;
    using database_row = std::map<std::string, database_value>;
    using database_result = std::vector<database_row>;

    namespace orm {
        // ORM framework (database/orm/)
        class entity_base { /* ... */ };
        class field_metadata { /* ... */ };
        class entity_metadata { /* ... */ };

        template<Entity EntityType>
        class query_builder { /* ... */ };

        // C++20 concepts
        template<typename T>
        concept Entity = requires(T t) { /* ... */ };

        template<typename T>
        concept FieldType = /* ... */;
    }

    namespace monitoring {
        // Performance monitoring (database/monitoring/)
        class performance_monitor { /* ... */ };
        class connection_metrics { /* ... */ };
        class query_metrics { /* ... */ };
        struct performance_summary { /* ... */ };
    }

    namespace security {
        // Security features (database/security/)
        class credential_manager { /* ... */ };
        class access_control { /* ... */ };
        class audit_logger { /* ... */ };
        enum class encryption_type { /* ... */ };
    }

    namespace async {
        // Asynchronous operations (database/async/)
        class async_database { /* ... */ };
        template<typename T>
        class database_awaitable { /* ... */ };
        class transaction_coordinator { /* ... */ };
        class stream_processor { /* ... */ };
    }

    namespace adapters {
        // Integration adapters (database/adapters/)
        #if KCENON_HAS_COMMON_SYSTEM
        class common_system_database_adapter { /* ... */ };
        class common_connection_pool_adapter { /* ... */ };
        #endif
    }

}
```

### Backend Implementations

Backend implementations extend the core `database::database_base` interface:

```cpp
namespace database {

    // PostgreSQL (database/postgres_manager.h)
    class postgres_manager : public database_base { /* ... */ };

    // MySQL (database/backends/mysql/)
    class mysql_manager : public database_base { /* ... */ };

    // SQLite (database/backends/sqlite/)
    class sqlite_manager : public database_base { /* ... */ };

    // MongoDB (database/backends/mongodb/)
    class mongodb_manager : public database_base { /* ... */ };

    // Redis (database/backends/redis/)
    class redis_manager : public database_base { /* ... */ };

}
```

---

## Backend-Specific Implementations

Each database backend is isolated in its own subdirectory with consistent file naming.

### PostgreSQL (Primary Backend)

**Location**: `database/postgres_manager.h`, `database/postgres_manager.cpp`

**Namespace**: `database::postgres_manager`

**Key Features**:
- libpqxx integration
- JSONB support
- Array data types
- Prepared statements
- Advanced indexing

**Dependencies**:
```cmake
find_package(PostgreSQL REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
target_link_libraries(database_system PRIVATE libpqxx::pqxx)
```

### MySQL Backend

**Location**: `database/backends/mysql/mysql_manager.h`, `mysql_manager.cpp`

**Namespace**: `database::mysql_manager`

**Key Features**:
- mysqlclient integration
- InnoDB support
- Full-text search
- Prepared statements

**Dependencies**:
```cmake
find_package(MySQL REQUIRED)
target_link_libraries(database_system PRIVATE MySQL::MySQL)
```

### SQLite Backend

**Location**: `database/backends/sqlite/sqlite_manager.h`, `sqlite_manager.cpp`

**Namespace**: `database::sqlite_manager`

**Key Features**:
- Embedded database
- WAL mode
- FTS5 support
- In-memory option

**Dependencies**:
```cmake
find_package(SQLite3 REQUIRED)
target_link_libraries(database_system PRIVATE SQLite::SQLite3)
```

### MongoDB Backend

**Location**: `database/backends/mongodb/mongodb_manager.h`, `mongodb_manager.cpp`

**Namespace**: `database::mongodb_manager`

**Key Features**:
- mongocxx driver
- Document operations
- Aggregation pipeline
- GridFS support

**Dependencies**:
```cmake
find_package(mongocxx CONFIG REQUIRED)
target_link_libraries(database_system PRIVATE mongo::mongocxx_shared)
```

### Redis Backend

**Location**: `database/backends/redis/redis_manager.h`, `redis_manager.cpp`

**Namespace**: `database::redis_manager`

**Key Features**:
- hiredis client
- All data types
- Pub/Sub messaging
- Transactions

**Dependencies**:
```cmake
find_package(hiredis CONFIG REQUIRED)
target_link_libraries(database_system PRIVATE hiredis::hiredis)
```

---

## Module Dependencies

### Dependency Graph

```
┌─────────────────────────────────────────────────┐
│           Application Layer                     │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│          database_manager.h                     │
│  (Singleton, mode switching, pooling)           │
└────┬───────────┬──────────────┬─────────────────┘
     │           │              │
     │           │              └──────────────────────┐
     │           │                                     │
┌────▼──────┐ ┌──▼──────────┐ ┌─────────────────┐ ┌──▼──────────┐
│connection │ │query_builder│ │  database_base  │ │ ORM modules │
│  _pool.h  │ │     .h      │ │       .h        │ │  (orm/)     │
└───────────┘ └─────────────┘ └────────┬────────┘ └─────────────┘
                                       │
                    ┌──────────────────┼──────────────────┐
                    │                  │                  │
            ┌───────▼────────┐ ┌───────▼────────┐ ┌──────▼────────┐
            │ postgres_      │ │  mysql_        │ │  sqlite_      │
            │  manager.h     │ │  manager.h     │ │  manager.h    │
            └────────────────┘ └────────────────┘ └───────────────┘
                    │                  │                  │
            ┌───────▼────────┐ ┌───────▼────────┐
            │ mongodb_       │ │  redis_        │
            │  manager.h     │ │  manager.h     │
            └────────────────┘ └────────────────┘
```

### Optional Dependencies

```
┌─────────────────────────────────────────────────┐
│          database_system (core)                 │
└────────────────────┬────────────────────────────┘
                     │
        ┌────────────┼────────────┬──────────────┐
        │            │            │              │
┌───────▼──────┐ ┌──▼────────┐ ┌─▼──────────┐ ┌─▼──────────┐
│ common_      │ │ thread_   │ │ logger_    │ │monitoring_ │
│  system      │ │  system   │ │  system    │ │  system    │
│ (Result<T>)  │ │ (async)   │ │ (logging)  │ │ (metrics)  │
└──────────────┘ └───────────┘ └────────────┘ └────────────┘
```

### Include Patterns

```cpp
// Core database functionality
#include <database/database_base.h>
#include <database/database_manager.h>
#include <database/database_types.h>
#include <database/connection_pool.h>
#include <database/query_builder.h>

// ORM framework
#include <database/orm/entity.h>

// Backend implementations
#include <database/postgres_manager.h>
#include <database/backends/mysql/mysql_manager.h>
#include <database/backends/sqlite/sqlite_manager.h>
#include <database/backends/mongodb/mongodb_manager.h>
#include <database/backends/redis/redis_manager.h>

// Advanced features
#include <database/monitoring/performance_monitor.h>
#include <database/security/secure_connection.h>
#include <database/async/async_operations.h>

// Optional integrations
#if KCENON_HAS_COMMON_SYSTEM
#include <database/adapters/common_system_adapter.h>
#endif
```

---

## File Organization Patterns

### Header Files (.h)

**Purpose**: Interface definitions, type declarations, inline functions

**Location**: `database/` and subdirectories

**Pattern**:
```cpp
#pragma once

// License header (BSD 3-Clause)
/*****************************************************************************
BSD 3-Clause License
...
*****************************************************************************/

// Include guards (using #pragma once)

// System includes
#include <string>
#include <vector>
#include <memory>

// Local includes
#include "database_types.h"

// Namespace declaration
namespace database {

    // Class/interface definition
    class example_class {
    public:
        // Public interface
        virtual ~example_class() = default;
        virtual bool method() = 0;

    private:
        // Private implementation
    };

}
```

### Implementation Files (.cpp)

**Purpose**: Function implementations, private helpers

**Location**: `database/` and subdirectories

**Pattern**:
```cpp
// License header (BSD 3-Clause)
/*****************************************************************************
BSD 3-Clause License
...
*****************************************************************************/

// Corresponding header
#include "example_class.h"

// Additional dependencies
#include <iostream>
#include <stdexcept>

// Namespace
namespace database {

    // Implementation
    bool example_class::method() {
        // Implementation details
        return true;
    }

}
```

### Test Files

**Purpose**: Unit and integration tests

**Location**: `tests/`, `integration_tests/`

**Pattern**:
```cpp
#include <gtest/gtest.h>
#include <database/database_manager.h>

class ExampleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }

    void TearDown() override {
        // Test cleanup
    }
};

TEST_F(ExampleTest, TestCase) {
    // Test implementation
    ASSERT_TRUE(true);
}
```

### CMake Configuration

**Main Configuration**: `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(database_system VERSION 1.0.0 LANGUAGES CXX)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_WITH_POSTGRESQL "Enable PostgreSQL support" ON)
option(BUILD_WITH_MYSQL "Enable MySQL support" OFF)
option(BUILD_WITH_SQLITE "Enable SQLite support" OFF)
option(BUILD_WITH_MONGODB "Enable MongoDB support" OFF)
option(BUILD_WITH_REDIS "Enable Redis support" OFF)
option(BUILD_WITH_COMMON_SYSTEM "Enable common_system integration" ON)

# Source files
set(DATABASE_SOURCES
    database/database_manager.cpp
    database/connection_pool.cpp
    database/query_builder.cpp
    database/postgres_manager.cpp
    database/orm/entity.cpp
    database/monitoring/performance_monitor.cpp
)

# Conditional backend sources
if(BUILD_WITH_MYSQL)
    list(APPEND DATABASE_SOURCES database/backends/mysql/mysql_manager.cpp)
endif()

if(BUILD_WITH_SQLITE)
    list(APPEND DATABASE_SOURCES database/backends/sqlite/sqlite_manager.cpp)
endif()

if(BUILD_WITH_MONGODB)
    list(APPEND DATABASE_SOURCES database/backends/mongodb/mongodb_manager.cpp)
endif()

if(BUILD_WITH_REDIS)
    list(APPEND DATABASE_SOURCES database/backends/redis/redis_manager.cpp)
endif()

# Library target
add_library(database_system ${DATABASE_SOURCES})

# Include directories
target_include_directories(database_system PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Dependencies
if(BUILD_WITH_POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
    find_package(libpqxx CONFIG REQUIRED)
    target_link_libraries(database_system PRIVATE libpqxx::pqxx)
    target_compile_definitions(database_system PRIVATE USE_POSTGRESQL=1)
endif()

# Export targets
install(TARGETS database_system
    EXPORT database_systemTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
```

---

## Conclusion

The Database System follows a clear, modular structure that promotes:

1. **Maintainability**: Logical file organization and namespace hierarchy
2. **Extensibility**: Easy addition of new backends and features
3. **Clarity**: Consistent naming and directory patterns
4. **Integration**: Clean separation of core and optional components

### Quick Reference

| Component | Location | Namespace |
|-----------|----------|-----------|
| Core abstractions | `database/` | `database::` |
| PostgreSQL (primary) | `database/postgres_manager.*` | `database::postgres_manager` |
| MySQL | `database/backends/mysql/` | `database::mysql_manager` |
| SQLite | `database/backends/sqlite/` | `database::sqlite_manager` |
| MongoDB | `database/backends/mongodb/` | `database::mongodb_manager` |
| Redis | `database/backends/redis/` | `database::redis_manager` |
| ORM | `database/orm/` | `database::orm::` |
| Monitoring | `database/monitoring/` | `database::monitoring::` |
| Security | `database/security/` | `database::security::` |
| Async | `database/async/` | `database::async::` |
| Adapters | `database/adapters/` | `database::adapters::` |

---

**Last Updated**: 2025-10-22
**Version**: 0.1.0.0
**Maintainer**: kcenon@naver.com
