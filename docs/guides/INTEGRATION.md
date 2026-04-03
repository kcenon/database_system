---
doc_id: "DBS-INTR-001"
doc_title: "Integration Guide - Database System"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "INTR"
---

# Integration Guide - Database System

> **SSOT**: This document is the single source of truth for **Integration Guide - Database System**.

> **Language:** **English** | [한국어](INTEGRATION.kr.md)

## Overview

This comprehensive guide describes how to integrate database_system with other modules in the ecosystem. Database System provides a unified abstraction layer for multiple database backends (PostgreSQL, SQLite, MongoDB, Redis) that seamlessly integrates with other system components.

**Version:** 0.1.0.0
**Last Updated:** 2025-10-22
**Architecture**: Multi-Backend Database Abstraction Layer

---

## Table of Contents

- [Quick Start](#quick-start)
- [Architecture Overview](#architecture-overview)
- [Integration with common_system](#integration-with-common_system)
- [Integration with container_system](#integration-with-container_system)
- [Integration with thread_system](#integration-with-thread_system)
- [Integration with logger_system](#integration-with-logger_system)
- [Integration with monitoring_system](#integration-with-monitoring_system)
- [Build Configuration](#build-configuration)
- [Database Backend Integration](#database-backend-integration)
- [ORM Framework Integration](#orm-framework-integration)
- [Performance Considerations](#performance-considerations)
- [Troubleshooting](#troubleshooting)
- [Examples](#examples)

---

## Quick Start

### Unified Database System (Recommended)

The unified_database_system provides the easiest way to get started with integrated logging, monitoring, and thread management:

```cpp
#include "integrated/unified_database_system.h"

using namespace database::integrated;

int main() {
    // 1. Zero-config initialization
    unified_database_system db;

    // 2. Connect to database
    auto conn_result = db.connect("host=localhost dbname=mydb user=myuser password=mypass");
    if (!conn_result) {
        std::cerr << "Connection failed: " << conn_result.error() << std::endl;
        return 1;
    }

    // 3. Execute query with automatic logging and monitoring
    auto result = db.execute("SELECT * FROM users WHERE id = $1", {42});
    if (result) {
        for (const auto& row : result->rows) {
            std::cout << "User: " << row["name"] << std::endl;
        }
    }

    // 4. Check health and metrics (built-in monitoring)
    auto health = db.check_health();
    auto metrics = db.get_metrics();

    std::cout << "Queries executed: " << metrics.total_queries << std::endl;
    std::cout << "Avg latency: " << metrics.avg_latency_ms << "ms" << std::endl;

    return 0;
}
```

**Benefits of unified_database_system**:
- ✅ Zero-configuration with smart defaults
- ✅ Integrated logging (logger_system or fallback)
- ✅ Built-in monitoring and metrics
- ✅ Thread pool for async operations
- ✅ Type-safe Result<T> pattern
- ✅ Fallback implementations (no external dependencies required)

### Legacy API: Basic Database Connection

```cpp
#include <database/database_manager.h>

int main() {
    // Create PostgreSQL database manager
    auto db = database_system::create_postgres_manager(
        "host=localhost dbname=mydb user=myuser password=mypass"
    );

    // Execute query
    auto result = db->execute_query("SELECT * FROM users WHERE id = $1", 42);

    if (result.is_ok()) {
        auto rows = result.value();
        for (const auto& row : rows) {
            std::cout << "User: " << row.get_string("name") << std::endl;
        }
    }

    return 0;
}
```

### CMake Integration

```cmake
find_package(database_system CONFIG REQUIRED)

add_executable(your_app main.cpp)

target_link_libraries(your_app PRIVATE
    kcenon::database_system
    PostgreSQL::PostgreSQL  # Or SQLite::SQLite3
)
```

---

## Architecture Overview

### Multi-Backend Architecture

Database System provides a unified interface across multiple database backends:

```
┌─────────────────────────────────────────────────────┐
│              Application Layer                      │
│         (Your Database Operations)                  │
└──────────────────────┬──────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────┐
│          database_system Abstraction Layer          │
│  ┌────────────────────────────────────────────┐    │
│  │      DatabaseManager (Unified Interface)   │    │
│  └────────┬───────────────────────────┬───────┘    │
│           │                           │             │
│  ┌────────▼───────┐         ┌────────▼───────┐    │
│  │ Connection Pool│         │  Query Builder │    │
│  └────────────────┘         └────────────────┘    │
└──────────────────────┬──────────────────────────────┘
                       │
        ┌──────────────┼──────────────┐
        │              │              │
┌───────▼─────┐ ┌──────▼────┐ ┌──────▼────┐
│ PostgreSQL  │ │  SQLite   │ │ MongoDB   │
│  Backend    │ │  Backend  │ │  Backend  │
└─────────────┘ └───────────┘ └───────────┘
        │              │              │
┌───────▼─────┐ ┌──────▼────┐
│  MongoDB    │ │   Redis   │
│  Backend    │ │  Backend  │
└─────────────┘ └───────────┘
```

### Key Components

```cpp
namespace database_system {

    // Core abstraction
    class database_manager {
    public:
        virtual Result<query_result> execute_query(
            const std::string& query,
            const std::vector<parameter>& params = {}) = 0;

        virtual Result<void> begin_transaction() = 0;
        virtual Result<void> commit_transaction() = 0;
        virtual Result<void> rollback_transaction() = 0;
    };

    // Connection pooling
    class connection_pool {
    public:
        auto acquire_connection() -> Result<connection_ptr>;
        void release_connection(connection_ptr conn);
        auto get_pool_stats() -> pool_statistics;
    };

    // Query builder
    class query_builder {
    public:
        auto select(const std::string& table) -> query_builder&;
        auto where(const std::string& condition) -> query_builder&;
        auto build() -> std::string;
    };

    // ORM Framework
    class entity_mapper {
    public:
        template<typename T>
        auto map_to_entity(const query_result& result) -> Result<T>;

        template<typename T>
        auto map_from_entity(const T& entity) -> Result<query_params>;
    };
}
```

---

## Integration with common_system

Database System integrates with common_system for standardized error handling and interfaces.

### Result<T> Pattern Usage

```cpp
#include <database/database_manager.h>
#include <kcenon/common/patterns/result.h>

// All database operations return Result<T>
auto fetch_user(std::shared_ptr<database_system::database_manager> db, int user_id)
    -> common::Result<User> {

    return db->execute_query("SELECT * FROM users WHERE id = $1", user_id)
        .and_then([](const auto& result) -> common::Result<User> {
            if (result.empty()) {
                return common::error<User>(
                    common::error_codes::NOT_FOUND,
                    "User not found",
                    "user_repository"
                );
            }

            return common::ok(User::from_row(result[0]));
        });
}

// Usage with monadic operations
auto user_result = fetch_user(db, 42)
    .map([](const User& user) {
        return user.with_normalized_name();
    })
    .and_then(validate_user)
    .or_else([](const auto& error) -> common::Result<User> {
        log_error(error);
        return load_default_user();
    });
```

### Error Code Integration

```cpp
#include <database/error_codes.h>
#include <kcenon/common/error/error_codes.h>

// Database-specific error codes extend common error codes
namespace database_system {
    enum class db_error_codes : int {
        CONNECTION_FAILED = -500,
        QUERY_ERROR = -501,
        TRANSACTION_FAILED = -502,
        CONSTRAINT_VIOLATION = -503,
        DEADLOCK_DETECTED = -504,
        // ... (range -500 to -599)
    };
}

// Convert to common error format
auto to_common_error(db_error_codes code, const std::string& message)
    -> common::error_info {
    return common::error_info{
        static_cast<int>(code),
        message,
        "database_system"
    };
}
```

### Build Configuration

```cmake
find_package(common_system CONFIG REQUIRED)
find_package(database_system CONFIG REQUIRED)

target_link_libraries(your_app PRIVATE
    kcenon::common_system
    kcenon::database_system
)

# Enable common_system integration
target_compile_definitions(your_app PRIVATE
    DATABASE_USE_COMMON_SYSTEM=1
)
```

---

## Integration with container_system

Store and retrieve container_system containers in databases.

### Binary Data Storage

```cpp
#include <database/database_manager.h>
#include <container/container.h>

// Store container as binary blob
auto store_container(
    std::shared_ptr<database_system::database_manager> db,
    const std::string& key,
    std::shared_ptr<container_system::container> container)
    -> common::Result<void> {

    auto binary_data = container->serialize_to_binary();

    return db->execute_query(
        "INSERT INTO containers (key, data) VALUES ($1, $2)",
        key, binary_data
    ).map([](const auto&) { /* void */ });
}

// Retrieve container from binary
auto load_container(
    std::shared_ptr<database_system::database_manager> db,
    const std::string& key)
    -> common::Result<container_ptr> {

    return db->execute_query(
        "SELECT data FROM containers WHERE key = $1",
        key
    ).and_then([](const auto& result) -> common::Result<container_ptr> {
        if (result.empty()) {
            return common::error<container_ptr>(
                common::error_codes::NOT_FOUND,
                "Container not found",
                "container_repository"
            );
        }

        auto binary = result[0].get_blob("data");
        return common::ok(container_system::deserialize_from_binary(binary));
    });
}
```

### JSON Storage (PostgreSQL JSONB)

```cpp
#include <database/postgres_manager.h>
#include <container/container.h>

// Store container as JSONB for queryable fields
auto store_container_json(
    std::shared_ptr<database_system::postgres_manager> db,
    const std::string& key,
    std::shared_ptr<container_system::container> container)
    -> common::Result<void> {

    auto json = container->serialize_to_json();

    return db->execute_query(
        "INSERT INTO containers (key, data) VALUES ($1, $2::jsonb)",
        key, json
    ).map([](const auto&) { /* void */ });
}

// Query JSON fields directly
auto find_containers_by_field(
    std::shared_ptr<database_system::postgres_manager> db,
    const std::string& field,
    const std::string& value)
    -> common::Result<std::vector<container_ptr>> {

    return db->execute_query(
        "SELECT data FROM containers WHERE data->>'{}' = $1",
        field, value
    ).map([](const auto& result) {
        std::vector<container_ptr> containers;
        for (const auto& row : result) {
            auto json = row.get_string("data");
            containers.push_back(container_system::deserialize_from_json(json));
        }
        return containers;
    });
}
```

### Build Configuration

```cmake
find_package(container_system CONFIG REQUIRED)
find_package(database_system CONFIG REQUIRED)

target_link_libraries(your_app PRIVATE
    kcenon::container_system
    kcenon::database_system
)
```

---

## Integration with thread_system

Database operations can be executed asynchronously using thread_system.

### Async Database Operations

```cpp
#include <database/database_manager.h>
#include <kcenon/thread/core/thread_pool.h>

class AsyncDatabaseService {
private:
    std::shared_ptr<database_system::database_manager> db_;
    std::shared_ptr<kcenon::thread::thread_pool> pool_;

public:
    AsyncDatabaseService(
        std::shared_ptr<database_system::database_manager> db,
        std::shared_ptr<kcenon::thread::thread_pool> pool)
        : db_(std::move(db)), pool_(std::move(pool)) {}

    // Async query execution
    auto execute_query_async(const std::string& query)
        -> std::future<common::Result<query_result>> {

        return pool_->post([this, query](const auto&) {
            return this->db_->execute_query(query);
        });
    }

    // Async transaction
    auto execute_transaction_async(
        std::vector<std::string> queries)
        -> std::future<common::Result<void>> {

        return pool_->post([this, queries = std::move(queries)](const auto&) {
            auto begin_result = db_->begin_transaction();
            if (!begin_result.is_ok()) {
                return common::Result<void>(begin_result.error());
            }

            for (const auto& query : queries) {
                auto result = db_->execute_query(query);
                if (!result.is_ok()) {
                    db_->rollback_transaction();
                    return common::Result<void>(result.error());
                }
            }

            return db_->commit_transaction();
        });
    }
};

// Usage
auto service = std::make_shared<AsyncDatabaseService>(db, pool);

auto future = service->execute_query_async("SELECT * FROM users");

// Continue with other work...

// Get result when needed
auto result = future.get();
if (result.is_ok()) {
    process_rows(result.value());
}
```

### Connection Pool with Thread Pool

```cpp
#include <database/connection_pool.h>
#include <kcenon/thread/core/thread_pool.h>

class DatabaseWorkerPool {
private:
    std::shared_ptr<database_system::connection_pool> conn_pool_;
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;

public:
    DatabaseWorkerPool(size_t pool_size, const std::string& conn_string) {
        conn_pool_ = std::make_shared<database_system::connection_pool>(
            pool_size,
            conn_string
        );

        thread_pool_ = std::make_shared<kcenon::thread::thread_pool>(pool_size);
    }

    // Execute query on any available worker
    auto execute(const std::string& query)
        -> std::future<common::Result<query_result>> {

        return thread_pool_->post([this, query](const auto&) {
            // Acquire connection from pool
            auto conn_result = conn_pool_->acquire_connection();
            if (!conn_result.is_ok()) {
                return common::Result<query_result>(conn_result.error());
            }

            auto conn = conn_result.value();

            // Execute query
            auto result = conn->execute(query);

            // Release connection back to pool
            conn_pool_->release_connection(conn);

            return result;
        });
    }
};
```

### Build Configuration

```cmake
find_package(thread_system CONFIG REQUIRED)
find_package(database_system CONFIG REQUIRED)

target_link_libraries(your_app PRIVATE
    kcenon::thread_system
    kcenon::database_system
)
```

---

## Integration with logger_system

Log database operations for debugging and auditing.

### Database Logger Integration

```cpp
#include <database/database_manager.h>
#include <kcenon/logger/core/logger.h>

class LoggingDatabaseManager : public database_system::database_manager {
private:
    std::shared_ptr<database_system::database_manager> db_;
    std::shared_ptr<kcenon::logger::logger> logger_;

public:
    LoggingDatabaseManager(
        std::shared_ptr<database_system::database_manager> db,
        std::shared_ptr<kcenon::logger::logger> logger)
        : db_(std::move(db)), logger_(std::move(logger)) {}

    auto execute_query(const std::string& query,
                      const std::vector<parameter>& params)
        -> common::Result<query_result> override {

        logger_->log(kcenon::logger::log_level::debug,
                    std::format("Executing query: {}", query));

        auto start = std::chrono::steady_clock::now();

        auto result = db_->execute_query(query, params);

        auto duration = std::chrono::steady_clock::now() - start;
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        if (result.is_ok()) {
            logger_->log(kcenon::logger::log_level::info,
                        std::format("Query completed in {} ms, {} rows affected",
                                   duration_ms.count(),
                                   result.value().row_count()));
        } else {
            logger_->log(kcenon::logger::log_level::error,
                        std::format("Query failed: {}",
                                   result.error().message));
        }

        return result;
    }

    // Forward other methods...
    auto begin_transaction() -> common::Result<void> override {
        logger_->log(kcenon::logger::log_level::debug, "Beginning transaction");
        return db_->begin_transaction();
    }

    auto commit_transaction() -> common::Result<void> override {
        logger_->log(kcenon::logger::log_level::debug, "Committing transaction");
        return db_->commit_transaction();
    }

    auto rollback_transaction() -> common::Result<void> override {
        logger_->log(kcenon::logger::log_level::warn, "Rolling back transaction");
        return db_->rollback_transaction();
    }
};

// Usage
auto db = database_system::create_postgres_manager(conn_string);
auto logger = kcenon::logger::logger_builder()
    .set_log_level(kcenon::logger::log_level::debug)
    .add_file_writer("database.log")
    .build();

auto logging_db = std::make_shared<LoggingDatabaseManager>(db, logger);

// All database operations now logged
logging_db->execute_query("SELECT * FROM users");
```

### Build Configuration

```cmake
find_package(logger_system CONFIG REQUIRED)
find_package(database_system CONFIG REQUIRED)

target_link_libraries(your_app PRIVATE
    kcenon::logger_system
    kcenon::database_system
)
```

---

## Integration with monitoring_system

Monitor database performance and collect metrics.

### Database Metrics Collection

```cpp
#include <database/database_manager.h>
#include <kcenon/monitoring/core/performance_monitor.h>

class MonitoredDatabaseManager : public database_system::database_manager {
private:
    std::shared_ptr<database_system::database_manager> db_;
    std::shared_ptr<kcenon::monitoring::performance_monitor> monitor_;

public:
    MonitoredDatabaseManager(
        std::shared_ptr<database_system::database_manager> db,
        std::shared_ptr<kcenon::monitoring::performance_monitor> monitor)
        : db_(std::move(db)), monitor_(std::move(monitor)) {}

    auto execute_query(const std::string& query,
                      const std::vector<parameter>& params)
        -> common::Result<query_result> override {

        // Record query attempt
        monitor_->record_counter("database.queries.total", 1);

        // Time query execution
        monitor_->start_timer("database.query.latency");

        auto result = db_->execute_query(query, params);

        auto latency = monitor_->stop_timer("database.query.latency");

        // Record result
        if (result.is_ok()) {
            monitor_->record_counter("database.queries.success", 1);
            monitor_->record_histogram("database.query.latency_ms", latency);
            monitor_->record_gauge("database.last_query_rows",
                                  result.value().row_count());
        } else {
            monitor_->record_counter("database.queries.failed", 1);
            monitor_->record_event("database.query.error");
        }

        return result;
    }

    // Monitor connection pool
    void report_pool_stats() {
        auto stats = db_->get_connection_pool()->get_stats();

        monitor_->record_gauge("database.pool.active", stats.active_connections);
        monitor_->record_gauge("database.pool.idle", stats.idle_connections);
        monitor_->record_gauge("database.pool.total", stats.total_connections);
        monitor_->record_gauge("database.pool.wait_queue", stats.wait_queue_size);
    }
};

// Collected Metrics:
// - database.queries.total (counter): Total queries executed
// - database.queries.success (counter): Successful queries
// - database.queries.failed (counter): Failed queries
// - database.query.latency_ms (histogram): Query execution time distribution
// - database.last_query_rows (gauge): Rows returned by last query
// - database.pool.active (gauge): Active connections
// - database.pool.idle (gauge): Idle connections
// - database.pool.total (gauge): Total connections in pool
// - database.pool.wait_queue (gauge): Queries waiting for connection
```

### Build Configuration

```cmake
find_package(monitoring_system CONFIG REQUIRED)
find_package(database_system CONFIG REQUIRED)

target_link_libraries(your_app PRIVATE
    kcenon::monitoring_system
    kcenon::database_system
)
```

---

## Build Configuration

### CMake Options

```cmake
# Database system options
option(BUILD_WITH_POSTGRESQL "Enable PostgreSQL support" ON)
option(BUILD_WITH_SQLITE "Enable SQLite support" OFF)
option(BUILD_WITH_MONGODB "Enable MongoDB support" OFF)
option(BUILD_WITH_REDIS "Enable Redis support" OFF)

# Feature options
option(ENABLE_ORM "Enable ORM framework" ON)
option(ENABLE_ASYNC_OPS "Enable async operations" ON)
option(ENABLE_CONNECTION_POOL "Enable connection pooling" ON)

# Integration options
option(BUILD_WITH_COMMON_SYSTEM "Enable common_system integration" ON)
option(BUILD_WITH_CONTAINER_SYSTEM "Enable container_system integration" OFF)
option(BUILD_WITH_THREAD_SYSTEM "Enable thread_system integration" OFF)
```

### Full Ecosystem Integration

```cmake
cmake_minimum_required(VERSION 3.16)
project(integrated_database_app)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find all required packages
find_package(common_system CONFIG REQUIRED)
find_package(thread_system CONFIG REQUIRED)
find_package(logger_system CONFIG REQUIRED)
find_package(monitoring_system CONFIG REQUIRED)
find_package(container_system CONFIG REQUIRED)
find_package(database_system CONFIG REQUIRED)

# Find database backend
find_package(PostgreSQL REQUIRED)

add_executable(my_app
    main.cpp
    database_service.cpp
)

target_link_libraries(my_app PRIVATE
    kcenon::common_system
    kcenon::thread_system
    kcenon::logger_system
    kcenon::monitoring_system
    kcenon::container_system
    kcenon::database_system
    PostgreSQL::PostgreSQL
)

# Enable all integrations
target_compile_definitions(my_app PRIVATE
    DATABASE_USE_COMMON_SYSTEM=1
    DATABASE_USE_CONTAINER_SYSTEM=1
    DATABASE_USE_THREAD_SYSTEM=1
)
```

---

## Database Backend Integration

### PostgreSQL

```cpp
#include <database/postgres_manager.h>

auto db = database_system::create_postgres_manager(
    "host=localhost port=5432 dbname=mydb user=myuser password=mypass"
);

// PostgreSQL-specific features
auto result = db->execute_query(
    "INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id",
    "John Doe", "john@example.com"
);

// Use JSONB
auto json_result = db->execute_query(
    "SELECT data FROM products WHERE data->>'category' = $1",
    "electronics"
);
```

### SQLite

```cpp
#include <database/sqlite_manager.h>

auto db = database_system::create_sqlite_manager("myapp.db");

// SQLite-specific features (in-memory)
auto memory_db = database_system::create_sqlite_manager(":memory:");
```

### MongoDB

```cpp
#include <database/mongodb_manager.h>

auto db = database_system::create_mongodb_manager(
    "mongodb://localhost:27017",
    "mydb"
);

// Document operations
auto result = db->find_documents("users", {{"age", {{"$gt", 18}}}});
```

### Redis

```cpp
#include <database/redis_manager.h>

auto db = database_system::create_redis_manager("redis://localhost:6379");

// Key-value operations
db->set("user:42:name", "John Doe");
auto name = db->get("user:42:name");
```

---

## ORM Framework Integration

### Entity Mapping

```cpp
#include <database/orm/entity.h>
#include <database/orm/entity_mapper.h>

// Define entity
struct User : public database_system::entity {
    int id;
    std::string name;
    std::string email;
    int age;

    // Map to database columns
    static auto get_table_name() -> std::string { return "users"; }

    static auto get_column_mappings() -> std::map<std::string, std::string> {
        return {
            {"id", "id"},
            {"name", "name"},
            {"email", "email"},
            {"age", "age"}
        };
    }
};

// Use entity mapper
auto mapper = std::make_shared<database_system::entity_mapper<User>>(db);

// Save entity
User user{0, "John Doe", "john@example.com", 30};
auto save_result = mapper->save(user);

// Load entity
auto load_result = mapper->find_by_id(42);
if (load_result.is_ok()) {
    User loaded_user = load_result.value();
}

// Query entities
auto users_result = mapper->find_where("age > ?", 18);
```

---

## Performance Considerations

### Connection Pooling

```cpp
// Configure connection pool size
auto pool = database_system::connection_pool(
    10,  // min connections
    100, // max connections
    conn_string
);

// Pool automatically manages connections
auto conn = pool.acquire_connection();
// Use connection...
pool.release_connection(conn);
```

### Prepared Statements

```cpp
// Use prepared statements for repeated queries
auto stmt = db->prepare("SELECT * FROM users WHERE id = $1");

for (int id : user_ids) {
    auto result = stmt->execute(id);
    process(result);
}
```

### Batch Operations

```cpp
// Batch inserts for better performance
db->begin_transaction();

for (const auto& user : users) {
    db->execute_query(
        "INSERT INTO users (name, email) VALUES ($1, $2)",
        user.name, user.email
    );
}

db->commit_transaction();
```

---

## Troubleshooting

### Common Issues

#### 1. Connection Pool Exhaustion

**Symptom**: Queries hang waiting for connections

**Solution**: Increase pool size or reduce connection hold time

```cpp
// Increase pool size
auto pool = database_system::connection_pool(
    20,   // min connections (increased)
    200,  // max connections (increased)
    conn_string
);

// Or use RAII for automatic release
{
    auto conn = pool.acquire_connection();
    // Connection automatically released when scope ends
}
```

#### 2. Transaction Deadlocks

**Symptom**: Transactions fail with deadlock errors

**Solution**: Use consistent lock ordering and retry logic

```cpp
auto execute_with_retry = [](auto db, auto query, int max_retries = 3) {
    for (int i = 0; i < max_retries; ++i) {
        auto result = db->execute_query(query);
        if (result.is_ok() || result.error().code != db_error_codes::DEADLOCK_DETECTED) {
            return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * (i + 1)));
    }
    return result;
};
```

---

## Integration Modes and Configuration

### Build Configuration

The system supports multiple integration modes via CMake flags:

```cmake
# Full integration (all external systems enabled)
cmake .. -DUSE_LOGGER_SYSTEM=ON -DUSE_MONITORING_SYSTEM=ON -DUSE_THREAD_SYSTEM=ON

# Fallback mode (no external dependencies)
cmake .. -DUSE_LOGGER_SYSTEM=OFF -DUSE_MONITORING_SYSTEM=OFF -DUSE_THREAD_SYSTEM=OFF

# Partial integration (e.g., monitoring only)
cmake .. -DUSE_LOGGER_SYSTEM=OFF -DUSE_MONITORING_SYSTEM=ON -DUSE_THREAD_SYSTEM=OFF
```

**Recommended**: Use fallback mode for production deployments until external systems are fully stabilized.

### Migration from Legacy API

To migrate from the legacy `database_manager` API to `unified_database_system`:

1. **Replace includes**:
   ```cpp
   // OLD
   #include <database/database_manager.h>

   // NEW
   #include "integrated/unified_database_system.h"
   ```

2. **Update initialization**:
   ```cpp
   // OLD
   database_manager& db = database_manager::handle();
   db.set_mode(database_types::postgres);
   db.create_connection_pool(database_types::postgres, pool_config);

   // NEW
   unified_database_system db = unified_database_system::builder()
       .with_connection_string("host=localhost dbname=mydb")
       .with_pool_size(10, 100)
       .build();
   ```

3. **Update query execution**:
   ```cpp
   // OLD
   auto result = db.execute_query("SELECT * FROM users");

   // NEW
   auto result = db.execute("SELECT * FROM users");
   if (result) {
       // Process result->rows
   }
   ```

See `samples/integrated/migration_from_legacy.cpp` for a complete migration example.

---

## Examples

### Complete Integration Example

See [samples/integration_example/](samples/integration_example/) for a complete application demonstrating:
- Database manager with all ecosystem integrations
- Async database operations with thread_system
- Container storage and retrieval
- Comprehensive logging and monitoring
- ORM framework usage

### Running Examples

```bash
cd database_system
mkdir build && cd build
cmake .. -DBUILD_SAMPLES=ON
make
./samples/integration_example
```

---

## Support

- **Documentation**: [docs/](docs/)
- **API Reference**: [docs/API_REFERENCE.md](docs/API_REFERENCE.md)
- **Build Guide**: [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md)
- **Issues**: [GitHub Issues](https://github.com/kcenon/database_system/issues)
- **Email**: kcenon@naver.com

---

**Last Updated**: 2025-10-22
**Maintainer**: kcenon@naver.com
**Version**: 0.1.0.0
