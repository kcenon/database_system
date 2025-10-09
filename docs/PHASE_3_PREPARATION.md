# Phase 3: Error Handling Preparation - database_system

**Version**: 1.0
**Date**: 2025-10-09
**Status**: Ready for Implementation

---

## Overview

This document outlines the migration path for database_system to adopt the centralized error handling from common_system Phase 3, replacing boolean returns and conditional Result wrappers with consistent Result<T> usage.

---

## Current State

### Error Handling Status

**Current Approach**:
- Boolean returns for most operations (`connect()`, `disconnect()`, `create_query()`)
- Unsigned int returns for row counts (`insert_query()`, `update_query()`)
- Conditional `BUILD_WITH_COMMON_SYSTEM` for Result<T> wrappers
- `nullptr` returns for failed connection acquisitions

**Example**:
```cpp
// Current: Boolean returns
bool connect(const std::string& connect_string);
bool disconnect(void);
bool create_query(const std::string& query_string);

// Current: Conditional wrappers
#ifdef BUILD_WITH_COMMON_SYSTEM
    common::VoidResult connect_result(const std::string& connect_string);
    common::VoidResult disconnect_result();
#endif

// Current: Nullable pointer returns
std::shared_ptr<connection_wrapper> acquire_connection();
```

---

## Migration Plan

### Phase 3.1: Import Error Codes

**Action**: Add common_system error code dependency

```cpp
#include <kcenon/common/error/error_codes.h>
#include <kcenon/common/patterns/result.h>

using namespace common;
using namespace common::error;
```

### Phase 3.2: Key API Migrations

#### Priority 1: Database Connection Operations (High Impact)

```cpp
// Before
bool connect(const std::string& connect_string);
bool disconnect(void);

// After
Result<void> connect(const std::string& connect_string);
Result<void> disconnect(void);
```

**Error Codes**:
- `codes::database_system::connection_failed`
- `codes::database_system::connection_lost`
- `codes::database_system::invalid_connection_string`

**Example Implementation**:
```cpp
Result<void> database_manager::connect(const std::string& connect_string) {
    if (connected_) {
        return error<std::monostate>(
            codes::common::already_exists,
            "Already connected to database",
            "database_manager"
        );
    }

    if (connect_string.empty()) {
        return error<std::monostate>(
            codes::database_system::invalid_connection_string,
            "Empty connection string",
            "database_manager"
        );
    }

    if (!database_) {
        return error<std::monostate>(
            codes::common::not_initialized,
            "Database mode not set",
            "database_manager"
        );
    }

    try {
        bool result = database_->connect(connect_string);
        if (!result) {
            return error<std::monostate>(
                codes::database_system::connection_failed,
                "Failed to establish connection",
                "database_manager",
                connect_string
            );
        }

        connected_ = true;
        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::database_system::connection_failed,
            "Connection exception",
            "database_manager",
            e.what()
        );
    }
}

Result<void> database_manager::disconnect() {
    if (!connected_) {
        return error<std::monostate>(
            codes::common::not_initialized,
            "Not connected to database",
            "database_manager"
        );
    }

    try {
        if (database_) {
            database_->disconnect();
        }
        connected_ = false;
        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::database_system::connection_lost,
            "Disconnect failed",
            "database_manager",
            e.what()
        );
    }
}
```

#### Priority 2: Query Operations

```cpp
// Before
bool create_query(const std::string& query_string);
unsigned int insert_query(const std::string& query_string);
unsigned int update_query(const std::string& query_string);
unsigned int delete_query(const std::string& query_string);
database_result select_query(const std::string& query_string);

// After
Result<void> create_query(const std::string& query_string);
Result<size_t> insert_query(const std::string& query_string);
Result<size_t> update_query(const std::string& query_string);
Result<size_t> delete_query(const std::string& query_string);
Result<database_result> select_query(const std::string& query_string);
```

**Error Codes**:
- `codes::database_system::query_failed`
- `codes::database_system::query_syntax_error`
- `codes::database_system::query_timeout`

**Example Implementation**:
```cpp
Result<size_t> database_manager::insert_query(const std::string& query_string) {
    if (!connected_) {
        return error<size_t>(
            codes::common::not_initialized,
            "Not connected to database",
            "database_manager"
        );
    }

    if (query_string.empty()) {
        return error<size_t>(
            codes::common::invalid_argument,
            "Empty query string",
            "database_manager"
        );
    }

    try {
        unsigned int affected_rows = database_->insert_query(query_string);
        return ok(static_cast<size_t>(affected_rows));
    } catch (const std::exception& e) {
        // Check if it's a syntax error
        std::string error_msg = e.what();
        if (error_msg.find("syntax") != std::string::npos ||
            error_msg.find("SQL") != std::string::npos) {
            return error<size_t>(
                codes::database_system::query_syntax_error,
                "SQL syntax error",
                "database_manager",
                query_string.substr(0, 100)
            );
        }

        return error<size_t>(
            codes::database_system::query_failed,
            "Insert query failed",
            "database_manager",
            e.what()
        );
    }
}

Result<database_result> database_manager::select_query(
    const std::string& query_string
) {
    if (!connected_) {
        return error<database_result>(
            codes::common::not_initialized,
            "Not connected to database",
            "database_manager"
        );
    }

    try {
        database_result results = database_->select_query(query_string);
        return ok(std::move(results));
    } catch (const std::exception& e) {
        return error<database_result>(
            codes::database_system::query_failed,
            "Select query failed",
            "database_manager",
            e.what()
        );
    }
}
```

#### Priority 3: Connection Pool Operations

```cpp
// Before
bool initialize();
std::shared_ptr<connection_wrapper> acquire_connection();
void release_connection(std::shared_ptr<connection_wrapper> connection);
void shutdown();

// After
Result<void> initialize();
Result<std::shared_ptr<connection_wrapper>> acquire_connection();
Result<void> release_connection(std::shared_ptr<connection_wrapper> connection);
Result<void> shutdown();
```

**Error Codes**:
- `codes::database_system::pool_exhausted`
- `codes::database_system::pool_shutdown`
- `codes::database_system::pool_timeout`

**Example Implementation**:
```cpp
Result<void> connection_pool::initialize() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (total_created_.load() > 0) {
        return error<std::monostate>(
            codes::common::already_exists,
            "Connection pool already initialized",
            "connection_pool"
        );
    }

    try {
        // Create minimum connections
        for (size_t i = 0; i < config_.min_connections; ++i) {
            auto conn = create_connection();
            if (!conn) {
                return error<std::monostate>(
                    codes::database_system::connection_failed,
                    "Failed to create initial connections",
                    "connection_pool",
                    std::to_string(i) + " of " + std::to_string(config_.min_connections)
                );
            }

            available_connections_.push(
                std::make_shared<connection_wrapper>(std::move(conn))
            );
            total_created_.fetch_add(1);
        }

        // Start maintenance thread
        shutdown_requested_ = false;
        maintenance_thread_ = std::thread(&connection_pool::maintenance_thread, this);

        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::common::internal_error,
            "Connection pool initialization failed",
            "connection_pool",
            e.what()
        );
    }
}

Result<std::shared_ptr<connection_wrapper>> connection_pool::acquire_connection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);

    if (shutdown_requested_) {
        return error<std::shared_ptr<connection_wrapper>>(
            codes::database_system::pool_shutdown,
            "Connection pool is shutting down",
            "connection_pool"
        );
    }

    // Wait for available connection
    bool acquired = pool_condition_.wait_for(
        lock,
        config_.acquire_timeout,
        [this] {
            return !available_connections_.empty() ||
                   (total_created_ < config_.max_connections) ||
                   shutdown_requested_;
        }
    );

    if (shutdown_requested_) {
        return error<std::shared_ptr<connection_wrapper>>(
            codes::database_system::pool_shutdown,
            "Connection pool shutting down",
            "connection_pool"
        );
    }

    if (!acquired && available_connections_.empty()) {
        failed_acquisitions_.fetch_add(1);
        return error<std::shared_ptr<connection_wrapper>>(
            codes::database_system::pool_timeout,
            "Connection acquisition timeout",
            "connection_pool",
            std::to_string(config_.acquire_timeout.count()) + "ms"
        );
    }

    // Try to get existing connection or create new one
    std::shared_ptr<connection_wrapper> conn;

    if (!available_connections_.empty()) {
        conn = available_connections_.front();
        available_connections_.pop();
    } else if (total_created_ < config_.max_connections) {
        try {
            auto new_conn = create_connection();
            if (!new_conn) {
                failed_acquisitions_.fetch_add(1);
                return error<std::shared_ptr<connection_wrapper>>(
                    codes::database_system::connection_failed,
                    "Failed to create new connection",
                    "connection_pool"
                );
            }

            conn = std::make_shared<connection_wrapper>(std::move(new_conn));
            total_created_.fetch_add(1);
        } catch (const std::exception& e) {
            failed_acquisitions_.fetch_add(1);
            return error<std::shared_ptr<connection_wrapper>>(
                codes::database_system::connection_failed,
                "Connection creation failed",
                "connection_pool",
                e.what()
            );
        }
    } else {
        failed_acquisitions_.fetch_add(1);
        return error<std::shared_ptr<connection_wrapper>>(
            codes::database_system::pool_exhausted,
            "Connection pool exhausted",
            "connection_pool",
            "Max: " + std::to_string(config_.max_connections)
        );
    }

    // Validate connection
    if (!conn->is_healthy()) {
        conn.reset();
        // Recursively retry
        return acquire_connection();
    }

    active_count_.fetch_add(1);
    successful_acquisitions_.fetch_add(1);
    conn->update_last_used();

    return ok(conn);
}

Result<void> connection_pool::release_connection(
    std::shared_ptr<connection_wrapper> connection
) {
    if (!connection) {
        return error<std::monostate>(
            codes::common::invalid_argument,
            "Null connection",
            "connection_pool"
        );
    }

    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (shutdown_requested_) {
        // Don't return to pool if shutting down
        active_count_.fetch_sub(1);
        return ok();
    }

    if (!connection->is_healthy()) {
        // Don't return unhealthy connections
        active_count_.fetch_sub(1);
        total_created_.fetch_sub(1);
        return ok();
    }

    available_connections_.push(connection);
    active_count_.fetch_sub(1);
    pool_condition_.notify_one();

    return ok();
}
```

#### Priority 4: Transaction Operations

```cpp
// New: Transaction support with Result<T>
class transaction {
public:
    Result<void> begin();
    Result<void> commit();
    Result<void> rollback();
};
```

**Error Codes**:
- `codes::database_system::transaction_failed`
- `codes::database_system::transaction_rolled_back`
- `codes::database_system::transaction_timeout`

---

## Migration Checklist

### Code Changes

- [ ] Add common_system error code includes
- [ ] Migrate `connect()` to Result<void>
- [ ] Migrate `disconnect()` to Result<void>
- [ ] Migrate query operations to Result<T>
- [ ] Migrate connection_pool::initialize() to Result<void>
- [ ] Migrate connection_pool::acquire_connection() to Result<shared_ptr<T>>
- [ ] Migrate connection_pool::release_connection() to Result<void>
- [ ] Add transaction support with Result<T>
- [ ] Remove conditional `BUILD_WITH_COMMON_SYSTEM` blocks
- [ ] Add error context (connection strings, query snippets, etc.)

### Test Updates

- [ ] Update unit tests for Result<T> APIs
- [ ] Add error case tests for each error code
- [ ] Test connection failures and timeouts
- [ ] Test invalid connection strings
- [ ] Test pool exhaustion scenarios
- [ ] Test query syntax errors
- [ ] Test transaction failures and rollbacks
- [ ] Verify error message quality

### Documentation

- [ ] Update API reference with Result<T> signatures
- [ ] Document error codes for each function
- [ ] Add migration examples
- [ ] Update integration examples
- [ ] Create error handling guide

---

## Example Migrations

### Example 1: Basic Connection

```cpp
// Usage before
database_manager& db = database_manager::handle();
if (!db.set_mode(database_types::postgresql)) {
    log_error("Failed to set mode");
    return;
}
if (!db.connect("host=localhost dbname=mydb")) {
    log_error("Connection failed");
    return;
}

// Usage after
database_manager& db = database_manager::handle();
if (!db.set_mode(database_types::postgresql)) {
    log_error("Failed to set mode");
    return;
}

auto result = db.connect("host=localhost dbname=mydb");
if (result.is_err()) {
    const auto& err = result.error();
    log_error("Connection failed: {} - {}", err.message,
              err.details.value_or(""));
    return;
}
```

### Example 2: Monadic Query Chaining

```cpp
// Chain database operations
auto result = db.connect(connection_string)
    .and_then([&](auto) {
        return db.insert_query("INSERT INTO users (name) VALUES ('Alice')");
    })
    .and_then([&](size_t inserted) {
        log_info("Inserted {} rows", inserted);
        return db.select_query("SELECT * FROM users WHERE name='Alice'");
    })
    .map([](database_result results) {
        // Process results
        return results.size();
    })
    .or_else([](const error_info& err) {
        log_error("Database operation failed: {}", err.message);
        return ok(size_t{0});
    });
```

### Example 3: Connection Pool with RAII

```cpp
// RAII-style connection guard
class connection_guard {
    std::shared_ptr<connection_pool_base> pool_;
    Result<std::shared_ptr<connection_wrapper>> conn_result_;

public:
    connection_guard(std::shared_ptr<connection_pool_base> pool)
        : pool_(pool), conn_result_(pool->acquire_connection()) {}

    ~connection_guard() {
        if (conn_result_.is_ok()) {
            pool_->release_connection(conn_result_.value());
        }
    }

    Result<connection_wrapper*> get() {
        if (conn_result_.is_err()) {
            return error<connection_wrapper*>(conn_result_.error());
        }
        return ok(conn_result_.value().get());
    }
};

// Usage
auto pool = connection_pool_manager::instance().get_pool(database_types::postgresql);
connection_guard guard(pool);

auto conn_result = guard.get();
if (conn_result.is_ok()) {
    auto* conn = conn_result.value();
    // Use connection
}
```

---

## Error Code Mapping

### Database System Error Codes (-500 to -599)

```cpp
namespace common::error::codes::database_system {
    // Connection errors (-500 to -519)
    constexpr int connection_failed = -500;
    constexpr int connection_lost = -501;
    constexpr int connection_timeout = -502;
    constexpr int invalid_connection_string = -503;

    // Pool errors (-520 to -539)
    constexpr int pool_exhausted = -520;
    constexpr int pool_shutdown = -521;
    constexpr int pool_timeout = -522;

    // Query errors (-540 to -559)
    constexpr int query_failed = -540;
    constexpr int query_syntax_error = -541;
    constexpr int query_timeout = -542;

    // Transaction errors (-560 to -579)
    constexpr int transaction_failed = -560;
    constexpr int transaction_rolled_back = -561;
    constexpr int transaction_timeout = -562;
}
```

### Error Messages

| Code | Message | When to Use |
|------|---------|-------------|
| connection_failed | "Database connection failed" | Connection error |
| invalid_connection_string | "Invalid connection string" | Malformed string |
| pool_exhausted | "Connection pool exhausted" | No connections available |
| pool_timeout | "Connection acquisition timeout" | Timeout waiting |
| query_failed | "Database query failed" | Query execution error |
| query_syntax_error | "SQL syntax error" | Invalid SQL |
| transaction_failed | "Transaction failed" | Transaction error |

---

## Testing Strategy

### Unit Tests

```cpp
TEST(DatabasePhase3, ConnectWithInvalidString) {
    database_manager db;
    db.set_mode(database_types::postgresql);

    auto result = db.connect("");

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(
        codes::database_system::invalid_connection_string,
        result.error().code
    );
}

TEST(DatabasePhase3, PoolExhaustion) {
    connection_pool_config cfg;
    cfg.max_connections = 2;
    cfg.acquire_timeout = std::chrono::milliseconds(100);

    connection_pool pool(database_types::postgresql, cfg, factory);
    pool.initialize();

    // Acquire all connections
    auto conn1 = pool.acquire_connection();
    auto conn2 = pool.acquire_connection();

    ASSERT_TRUE(conn1.is_ok());
    ASSERT_TRUE(conn2.is_ok());

    // Try to acquire when exhausted
    auto conn3 = pool.acquire_connection();

    ASSERT_TRUE(conn3.is_err());
    EXPECT_EQ(
        codes::database_system::pool_timeout,
        conn3.error().code
    );
}

TEST(DatabasePhase3, QuerySyntaxError) {
    database_manager db;
    db.set_mode(database_types::sqlite);
    db.connect(":memory:");

    auto result = db.insert_query("INVALID SQL HERE");

    ASSERT_TRUE(result.is_err());
    // Could be query_failed or query_syntax_error depending on detection
}
```

### Integration Tests

```cpp
TEST(DatabasePhase3, FullWorkflow) {
    database_manager& db = database_manager::handle();
    db.set_mode(database_types::sqlite);

    // Connect
    auto connect_result = db.connect(":memory:");
    ASSERT_TRUE(connect_result.is_ok());

    // Create table
    auto create_result = db.create_query(
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)"
    );
    ASSERT_TRUE(create_result.is_ok());

    // Insert
    auto insert_result = db.insert_query(
        "INSERT INTO users (name) VALUES ('Alice')"
    );
    ASSERT_TRUE(insert_result.is_ok());
    EXPECT_EQ(1, insert_result.value());

    // Select
    auto select_result = db.select_query("SELECT * FROM users");
    ASSERT_TRUE(select_result.is_ok());
    EXPECT_EQ(1, select_result.value().size());

    // Disconnect
    auto disconnect_result = db.disconnect();
    ASSERT_TRUE(disconnect_result.is_ok());
}
```

---

## Performance Impact

### Expected Overhead

- **Result<T> size**: +24 bytes per return value (variant overhead)
- **Success path**: ~0-1ns (inline optimization)
- **Error path**: ~2-3ns (error_info construction)

### Current Performance (Baseline)

- **Connection establishment**: ~5ms (network dependent)
- **Simple query**: ~100μs (database dependent)
- **Pool acquire (fast path)**: ~50ns
- **Pool acquire (slow path)**: Up to timeout (5000ms default)

### Expected After Migration

- **Connection establishment**: ~5ms (no change, network is bottleneck)
- **Simple query**: ~101μs (-1% due to Result wrapping)
- **Pool acquire (fast path)**: ~52ns (-4%)
- **Pool acquire (slow path)**: Up to timeout (no change)

**Conclusion**: Negligible performance impact (<2% for most operations)

---

## Implementation Timeline

### Week 1: Foundation
- Day 1-2: Migrate connection operations
- Day 3: Migrate query operations
- Day 4-5: Update tests

### Week 2: Connection Pool
- Day 1-3: Migrate connection pool operations
- Day 4-5: Add transaction support

### Week 3: Finalization
- Day 1-2: Migrate remaining backend-specific code
- Day 3: Integration tests and performance validation
- Day 4-5: Documentation and code review

---

## Backwards Compatibility

### Strategy: Remove Conditional Compilation

The current code uses `BUILD_WITH_COMMON_SYSTEM` to conditionally provide Result wrappers:

```cpp
// Before: Conditional
#ifdef BUILD_WITH_COMMON_SYSTEM
    common::VoidResult connect_result(const std::string& connect_string);
#endif

// After: Make Result<T> primary
Result<void> connect(const std::string& connect_string);

// Provide deprecated boolean version temporarily
[[deprecated("Use Result<void> connect() instead")]]
bool connect_legacy(const std::string& connect_string) {
    auto result = connect(connect_string);
    return result.is_ok();
}
```

**Timeline**:
- Phase 3.1: Replace boolean APIs with Result<T>
- Phase 3.2: Deprecate old APIs
- Phase 4: Remove deprecated APIs (after 6 months)

---

## Special Considerations

### Connection Pool Lifecycle

The connection pool has complex lifecycle management with background threads. Ensure proper cleanup:

```cpp
Result<void> connection_pool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        if (shutdown_requested_) {
            return error<std::monostate>(
                codes::common::already_exists,
                "Already shutting down",
                "connection_pool"
            );
        }
        shutdown_requested_ = true;
    }

    pool_condition_.notify_all();
    maintenance_cv_.notify_all();

    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }

    std::lock_guard<std::mutex> lock(pool_mutex_);
    while (!available_connections_.empty()) {
        available_connections_.pop();
    }

    return ok();
}
```

### Database-Specific Errors

Different database backends may throw different exceptions. Map them to appropriate error codes:

```cpp
Result<size_t> backend_query(const std::string& query) {
    try {
        // Execute query
    } catch (const pqxx::sql_error& e) {
        // PostgreSQL syntax error
        return error<size_t>(
            codes::database_system::query_syntax_error,
            e.what(),
            "postgres_backend"
        );
    } catch (const pqxx::broken_connection& e) {
        return error<size_t>(
            codes::database_system::connection_lost,
            e.what(),
            "postgres_backend"
        );
    } catch (const std::exception& e) {
        return error<size_t>(
            codes::database_system::query_failed,
            e.what(),
            "postgres_backend"
        );
    }
}
```

---

## References

- [common_system Error Codes](../../common_system/include/kcenon/common/error/error_codes.h)
- [Error Handling Guidelines](../../common_system/docs/ERROR_HANDLING.md)
- [Result<T> Implementation](../../common_system/include/kcenon/common/patterns/result.h)
- [Database System RAII Guidelines](RAII_GUIDELINES.md)

---

**Document Status**: Phase 3 Preparation Complete
**Next Action**: Begin implementation or await approval
**Maintainer**: database_system team
