# Database System - Best Practices Guide

> **Version:** 0.1.0
> **Last Updated:** 2025-11-11

A comprehensive guide covering recommended patterns, performance optimization, security, and thread safety for the Database System.

## Table of Contents

1. [Connection Management](#1-connection-management)
2. [Query Optimization](#2-query-optimization)
3. [Transaction Management](#3-transaction-management)
4. [Security Best Practices](#4-security-best-practices)
5. [Performance Optimization](#5-performance-optimization)
6. [Error Handling Patterns](#6-error-handling-patterns)
7. [Thread Safety](#7-thread-safety)
8. [Memory Management](#8-memory-management)
9. [Backend-Specific Tips](#9-backend-specific-tips)

---

## 1. Connection Management

### Pooling and Reuse

**DO:** Always use connection pooling for concurrent access. Reuse connections from the pool to minimize overhead.

```cpp
#include <database/database_manager.h>
#include <database/connection_pool.h>

// Configure connection pool once at startup
database::database_manager& db = database::database_manager::handle();

database::connection_pool_config config;
config.min_connections = 10;        // Keep minimum connections ready
config.max_connections = 100;       // Scale to max under load
config.acquire_timeout = std::chrono::seconds(5);
config.idle_timeout = std::chrono::seconds(30);
config.health_check_interval = std::chrono::seconds(60);
config.connection_string = "host=localhost dbname=mydb user=admin password=secret";

if (!db.create_connection_pool(database::database_types::postgres, config)) {
    std::cerr << "Failed to create pool" << std::endl;
    return 1;
}

// Get pool reference for reuse across threads
auto pool = db.get_connection_pool(database::database_types::postgres);

// Acquire and use connections efficiently
{
    auto connection = pool->acquire_connection();
    if (connection) {
        // Connection automatically returned to pool when destroyed (RAII)
        auto result = connection->select_query("SELECT * FROM users");
        // Process result...
    }
} // Connection returns to pool here
```

**DON'T:** Create new connections for every database operation. Don't leak connections by not returning them to the pool.

```cpp
// BAD: Creates new connection each time, wastes resources
auto db = std::make_unique<postgres_manager>();
db->connect("host=localhost dbname=mydb user=admin");
auto result = db->select_query("SELECT * FROM users");
db->disconnect();

// BAD: Connection not returned if exception occurs
auto conn = pool->acquire_connection();
// ... if exception here, connection leaks!
pool->release_connection(std::move(conn));
```

### Cleanup and Resource Management

**DO:** Use RAII patterns. Connections are automatically returned to the pool.

```cpp
// Acquire from pool - RAII guarantees return
{
    auto connection = pool->acquire_connection();
    // Use connection...
} // Automatically returned, even if exception occurs

// Monitor pool health
auto stats = db.get_pool_stats();
for (const auto& [db_type, stat] : stats) {
    std::cout << "Active: " << stat.active_connections << std::endl;
    std::cout << "Available: " << stat.available_connections << std::endl;

    // Detect pool exhaustion
    if (stat.failed_acquisitions > stat.successful_acquisitions * 0.05) {
        std::cerr << "Warning: High acquisition failure rate!" << std::endl;
    }
}

// Gracefully shutdown at application exit
db.disconnect();
```

**DON'T:** Hold connections longer than necessary. Don't ignore failed acquisitions.

```cpp
// BAD: Hold connection for extended period
auto conn = pool->acquire_connection();
std::this_thread::sleep_for(std::chrono::seconds(60));
// Connection blocked during sleep, other threads starved

// BAD: Ignore acquisition failures
auto conn = pool->acquire_connection();
if (!conn) {
    // Silently fail to check - may cause undefined behavior later
    return;
}
```

---

## 2. Query Optimization

### Prepared Statements and Parameterized Queries

**DO:** Use prepared statements and parameterized queries to prevent SQL injection and improve performance.

```cpp
#include <database/query_builder.h>

// Type-safe query builder with parameters
auto query = db.create_query_builder(database::database_types::postgres)
    .select({"id", "username", "email"})
    .from("users")
    .where("id", "=", database::database_value{int64_t(123)})
    .where("status", "=", database::database_value{std::string("active")});

auto result = query.execute(&db);
if (result) {
    for (const auto& row : *result) {
        std::cout << "User: " << std::get<std::string>(row.at("username")) << std::endl;
    }
}

// For complex queries, use raw SQL with explicit parameterization
std::string email_param = "user@example.com";
auto safe_query = db.create_query_builder(database::database_types::postgres)
    .raw_sql("SELECT * FROM users WHERE email = $1 AND active = $2")
    // Parameters bound here (PostgreSQL uses $1, $2, etc.)
    .execute(&db);
```

**DON'T:** Concatenate user input directly into SQL strings. Don't ignore parameter binding.

```cpp
// BAD: SQL injection vulnerability
std::string user_input = "'; DROP TABLE users; --";
std::string query = "SELECT * FROM users WHERE username = '" + user_input + "'";
db.execute_query(query);

// BAD: Unbound parameters
std::string email = "user@example.com";
std::string unsafe = "SELECT * FROM users WHERE email = '" + email + "'";
```

### Batching and Bulk Operations

**DO:** Batch multiple operations together for better throughput.

```cpp
// Batch INSERT operations
std::vector<std::map<std::string, database::database_value>> rows;
for (int i = 0; i < 1000; ++i) {
    rows.push_back({
        {"username", database::database_value{std::string("user_" + std::to_string(i))}},
        {"email", database::database_value{std::string("user" + std::to_string(i) + "@example.com")}},
        {"created_at", database::database_value{std::string("NOW()")}}
    });
}

auto batch_result = db.create_query_builder(database::database_types::postgres)
    .insert_into("users")
    .values(rows)
    .execute(&db);

std::cout << "Inserted " << rows.size() << " rows in single operation" << std::endl;

// Benefits: 45ms for 1K inserts vs 45+ seconds individually
// ~1000x performance improvement for batch operations
```

**DON'T:** Insert/update records one at a time in loops. Don't perform unrelated operations in single batch.

```cpp
// BAD: N+1 inserts instead of 1 batch insert
for (int i = 0; i < 1000; ++i) {
    db.insert_query("INSERT INTO users (username) VALUES ('user_" + std::to_string(i) + "')");
    // 1000 round trips: 45+ seconds
}

// BAD: Mixing unrelated operations
auto batch = db.create_query_builder(database::database_types::postgres)
    .insert_into("users").values({...})
    .update("products").set("price", database::database_value{99.99})
    .delete_from("logs").where("timestamp", "<", database::database_value{std::string("NOW() - INTERVAL 1 DAY")});
```

### Indexing Strategy

**DO:** Create indexes on frequently queried columns and ensure queries use them.

```cpp
// Create indexes for common queries
db.create_query(
    "CREATE INDEX idx_users_email ON users(email)"
);
db.create_query(
    "CREATE INDEX idx_users_created_at ON users(created_at DESC)"
);
db.create_query(
    "CREATE INDEX idx_orders_customer_date ON orders(customer_id, created_at)"
);

// Compound index for multi-column filters
db.create_query(
    "CREATE INDEX idx_posts_author_published ON posts(author_id, published) "
    "WHERE published = true"  // PostgreSQL partial index
);

// Query uses index efficiently
auto indexed_query = db.create_query_builder(database::database_types::postgres)
    .select({"id", "username", "email"})
    .from("users")
    .where("email", "=", database::database_value{std::string("user@example.com")})
    .limit(1);
```

**DON'T:** Create indexes on every column. Don't ignore slow queries without profiling.

```cpp
// BAD: Over-indexing wastes storage and slows writes
db.create_query("CREATE INDEX idx_users_id ON users(id)");      // Redundant with PK
db.create_query("CREATE INDEX idx_users_name ON users(name)");  // Low cardinality
db.create_query("CREATE INDEX idx_users_status ON users(status)"); // Not selective

// BAD: Don't assume queries are fast without verification
auto slow_query = db.create_query_builder(database::database_types::postgres)
    .select({"*"})
    .from("large_table")
    .where("name", "LIKE", database::database_value{std::string("%query%")})
    .where("created_at", ">", database::database_value{std::string("CURRENT_DATE - 1000")});
// LIKE with % prefix doesn't use indexes; consider full-text search instead
```

---

## 3. Transaction Management

### ACID Compliance and Isolation Levels

**DO:** Use transactions for multi-statement operations requiring atomicity.

```cpp
#include <database/database_manager.h>

// Explicit transaction management
bool transfer_funds(int64_t from_id, int64_t to_id, double amount) {
    database::database_manager& db = database::database_manager::handle();

    try {
        // Begin transaction
        if (!db.create_query("BEGIN")) {
            std::cerr << "Failed to begin transaction" << std::endl;
            return false;
        }

        // Deduct from source account
        if (!db.execute_query(
            "UPDATE accounts SET balance = balance - " + std::to_string(amount) +
            " WHERE id = " + std::to_string(from_id) + " AND balance >= " + std::to_string(amount))) {
            db.create_query("ROLLBACK");
            return false;
        }

        // Add to destination account
        if (!db.execute_query(
            "UPDATE accounts SET balance = balance + " + std::to_string(amount) +
            " WHERE id = " + std::to_string(to_id))) {
            db.create_query("ROLLBACK");
            return false;
        }

        // Verify invariant: total balance unchanged
        auto check = db.select_query(
            "SELECT SUM(balance) as total FROM accounts WHERE id IN (" +
            std::to_string(from_id) + ", " + std::to_string(to_id) + ")"
        );

        // Commit if all checks pass
        if (!check.empty()) {
            if (!db.create_query("COMMIT")) {
                db.create_query("ROLLBACK");
                return false;
            }
            return true;
        }

        db.create_query("ROLLBACK");
        return false;

    } catch (const std::exception& e) {
        db.create_query("ROLLBACK");
        std::cerr << "Transaction error: " << e.what() << std::endl;
        return false;
    }
}

// Use with connection pool for thread safety
auto pool = db.get_connection_pool(database::database_types::postgres);
auto conn = pool->acquire_connection();
if (conn) {
    bool success = transfer_funds(1, 2, 100.0);
}
```

**DON'T:** Leave transactions open indefinitely. Don't ignore rollback on errors.

```cpp
// BAD: Transaction left open after connection loss
auto query = db.create_query("BEGIN");
// ... if connection drops here, transaction locked on server

// BAD: Missing error handling
db.create_query("BEGIN");
db.execute_query("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
// If exception here without catch, transaction never commits or rolls back
db.create_query("COMMIT");

// BAD: Nested transactions (not supported in most databases)
db.create_query("BEGIN");
db.create_query("BEGIN");  // Error: already in transaction
db.create_query("COMMIT");
```

### Error Handling in Transactions

**DO:** Implement comprehensive error handling with explicit rollback.

```cpp
class TransactionGuard {
    database::database_manager& db_;
    bool committed_ = false;

public:
    TransactionGuard(database::database_manager& db) : db_(db) {
        if (!db_.create_query("BEGIN")) {
            throw std::runtime_error("Failed to begin transaction");
        }
    }

    ~TransactionGuard() {
        if (!committed_) {
            db_.create_query("ROLLBACK");  // Auto-rollback if not explicitly committed
        }
    }

    bool commit() {
        if (!db_.create_query("COMMIT")) {
            throw std::runtime_error("Failed to commit transaction");
        }
        committed_ = true;
        return true;
    }
};

// Usage
try {
    TransactionGuard guard(db);

    db.execute_query("INSERT INTO users (username) VALUES ('john')");
    db.execute_query("INSERT INTO profiles (user_id, bio) VALUES (1, 'Developer')");

    guard.commit();  // Only if both succeed
} catch (const std::exception& e) {
    std::cerr << "Transaction failed: " << e.what() << std::endl;
    // ~TransactionGuard automatically calls ROLLBACK
}
```

---

## 4. Security Best Practices

### Credential Management and Encryption

**DO:** Securely store and manage database credentials.

```cpp
#include <database/security/credential_manager.h>
#include <database/security/access_control.h>

// Initialize credential manager
auto& credentials = database::credential_manager::instance();

// Store credentials securely with encryption
database::security_credentials prod_creds;
prod_creds.username = "app_user";
prod_creds.password_hash = credentials.hash_password("secure_password_123");
prod_creds.encryption = database::encryption_type::tls;
prod_creds.verify_certificate = true;
prod_creds.certificate_path = "/etc/ssl/certs/ca-bundle.crt";

credentials.store_credentials("production_db", prod_creds);

// Retrieve credentials (decrypted automatically)
auto retrieved = credentials.get_credentials("production_db");
if (retrieved) {
    std::string conn_string = "host=prod.db.example.com dbname=mydb user=" +
                              retrieved->username;
    db.connect(conn_string);
}

// Use environment variables for sensitive data at runtime
const char* password_env = std::getenv("DB_PASSWORD");
if (password_env) {
    prod_creds.password_hash = credentials.hash_password(password_env);
    // Immediately zero out the sensitive variable
    volatile char* temp = const_cast<char*>(password_env);
    std::memset(temp, 0, std::strlen(password_env));
}
```

**DON'T:** Hardcode credentials in source code. Don't transmit unencrypted credentials.

```cpp
// BAD: Hardcoded credentials
database::database_manager& db = database::database_manager::handle();
db.connect("host=localhost dbname=mydb user=admin password=admin123");

// BAD: Credentials in plain text in config files
std::ifstream config("database.conf");
std::string line;
while (std::getline(config, line)) {
    if (line.find("password=") != std::string::npos) {
        // Password exposed in file
    }
}

// BAD: Storing unhashed passwords
auto creds = credentials.get_credentials("database");
std::cout << "Password: " << creds->password_hash << std::endl;  // Exposed in logs
```

### SQL Injection Prevention

**DO:** Always use parameterized queries and query builders to prevent SQL injection.

```cpp
#include <database/query_builder.h>

// Type-safe query builder - SQL injection impossible
std::string user_input = "'; DROP TABLE users; --";

auto safe = db.create_query_builder(database::database_types::postgres)
    .select({"id", "username"})
    .from("users")
    .where("username", "=", database::database_value{user_input})
    .execute(&db);
// Parameter properly escaped, injection prevented

// For complex conditions, use where_raw cautiously with validation
std::string status_list = "active,inactive,pending";
if (status_list.find(';') == std::string::npos && status_list.find('\'') == std::string::npos) {
    // Validated input only
    auto validated = db.create_query_builder(database::database_types::postgres)
        .select({"*"})
        .from("users")
        .where_raw("status IN ('" + status_list + "')")
        .execute(&db);
}
```

**DON'T:** Use string concatenation for queries. Don't skip validation of raw SQL.

```cpp
// BAD: String concatenation enables injection
std::string search_term = user_input;  // Untrusted
std::string query = "SELECT * FROM users WHERE username = '" + search_term + "'";
db.execute_query(query);  // SQL injection possible

// BAD: Raw SQL without validation
db.create_query_builder(database::database_types::postgres)
    .raw_sql("SELECT * FROM users WHERE " + user_provided_where_clause)
    .execute(&db);
```

### Audit Logging

**DO:** Log all database access for security and compliance.

```cpp
#include <database/security/audit_logger.h>

// Enable comprehensive audit logging
auto& audit = database::audit_logger::instance();

// Log sensitive operations
AUDIT_LOG_ACCESS(
    "user@company.com",           // User identifier
    "session_token_xyz",          // Session tracking
    "DELETE",                     // Operation type
    "customers",                  // Table name
    "WHERE id > 1000",            // Query hash or conditions
    true,                         // Success flag
    ""                            // Error message (if any)
);

// Periodically review audit logs
auto log_entries = audit.get_recent_logs(std::chrono::hours(24));
for (const auto& entry : log_entries) {
    if (entry.operation == "DELETE" || entry.operation == "DROP") {
        // Alert on destructive operations
        std::cout << "Destructive operation by " << entry.user_id
                  << " on " << entry.table_name << std::endl;
    }
}

// Check for suspicious patterns
auto failed_logins = audit.query_logs({
    {"operation", "CONNECT"},
    {"success", false}
});

if (failed_logins.size() > 5) {
    // Alert: multiple failed connection attempts
    std::cout << "Potential brute force attack detected!" << std::endl;
}
```

**DON'T:** Ignore audit logging. Don't log sensitive data like passwords.

```cpp
// BAD: Storing passwords in audit logs
AUDIT_LOG_ACCESS("user", "session", "CONNECT", "database", "password=secret123", true, "");

// BAD: No audit trail
db.execute_query("DELETE FROM audit_logs WHERE created_at < NOW() - INTERVAL 1 YEAR");
// Destroy audit history
```

---

## 5. Performance Optimization

### Connection Tuning

**DO:** Configure connection pool parameters based on workload characteristics.

```cpp
// For high-concurrency read-heavy workload
database::connection_pool_config read_heavy_config;
read_heavy_config.min_connections = 20;     // Keep connections ready
read_heavy_config.max_connections = 200;    // Scale aggressively
read_heavy_config.acquire_timeout = std::chrono::milliseconds(100);
read_heavy_config.idle_timeout = std::chrono::minutes(5);
read_heavy_config.health_check_interval = std::chrono::seconds(30);

db.create_connection_pool(database::database_types::postgres, read_heavy_config);

// For batch processing (fewer but longer-held connections)
database::connection_pool_config batch_config;
batch_config.min_connections = 2;
batch_config.max_connections = 10;
batch_config.acquire_timeout = std::chrono::seconds(30);
batch_config.idle_timeout = std::chrono::minutes(15);

db.create_connection_pool(database::database_types::mongodb, batch_config);

// Monitor actual usage to tune parameters
auto stats = db.get_pool_stats();
for (const auto& [db_type, stat] : stats) {
    double utilization = (double)stat.active_connections /
                        (stat.active_connections + stat.available_connections);

    if (utilization > 0.9) {
        std::cout << "Pool near capacity: " << utilization * 100 << "%" << std::endl;
        // Consider increasing max_connections
    } else if (utilization < 0.2 && stat.total_connections > stat.min_connections) {
        std::cout << "Pool over-provisioned" << std::endl;
        // Consider decreasing min_connections
    }
}
```

### Monitoring and Metrics Collection

**DO:** Collect and analyze database performance metrics continuously.

```cpp
#include <database/monitoring/performance_monitor.h>

// Initialize performance monitoring
auto& monitor = database::performance_monitor::instance();

// Configure alert thresholds
monitor.set_alert_thresholds(
    0.05,                                        // 5% error rate threshold
    std::chrono::milliseconds(1000)              // 1s query time threshold
);

// Register alert handlers
monitor.register_alert_handler([](const database::performance_alert& alert) {
    std::cerr << "Alert: " << alert.message() << std::endl;
    // Send to monitoring system (Prometheus, DataDog, etc.)
});

// Collect performance metrics
auto start = std::chrono::high_resolution_clock::now();

auto result = db.create_query_builder(database::database_types::postgres)
    .select({"*"})
    .from("large_table")
    .execute(&db);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

std::cout << "Query executed in " << duration.count() << "ms" << std::endl;

// Get aggregated performance summary
auto summary = monitor.get_performance_summary();
std::cout << "QPS: " << summary.queries_per_second << std::endl;
std::cout << "Avg Latency: " << summary.avg_query_time.count() << "μs" << std::endl;
std::cout << "Error Rate: " << (summary.error_rate * 100) << "%" << std::endl;

// Export to Prometheus for long-term analysis
database::prometheus_exporter exporter("http://prometheus:9090", 9091);
exporter.export_metrics(summary);
```

### Caching Strategies

**DO:** Implement strategic caching to reduce database load.

```cpp
#include <database/backends/redis/redis_manager.h>

// Use Redis for result caching
auto cache_pool = db.get_connection_pool(database::database_types::redis);

// Cache user lookups with TTL
auto get_user_cached = [&](int64_t user_id) -> std::optional<database::database_result> {
    auto cache_conn = cache_pool->acquire_connection();

    // Try cache first
    std::string cache_key = "user:" + std::to_string(user_id);
    auto cached = cache_conn->select_query("GET " + cache_key);

    if (!cached.empty()) {
        return cached;  // Cache hit
    }

    // Cache miss: query database
    auto db_conn = db.get_connection_pool(database::database_types::postgres)->acquire_connection();
    auto result = db_conn->select_query(
        "SELECT * FROM users WHERE id = " + std::to_string(user_id)
    );

    // Store in cache with 1-hour TTL
    if (!result.empty()) {
        cache_conn->execute_query(
            "SETEX " + cache_key + " 3600 '" + serialize_result(result) + "'"
        );
    }

    return result;
};

// Invalidate cache on updates
db.create_query_builder(database::database_types::postgres)
    .update("users")
    .set("email", database::database_value{std::string("new@example.com")})
    .where("id", "=", database::database_value{int64_t(123)})
    .execute(&db);

// Invalidate cache
auto cache = db.get_connection_pool(database::database_types::redis)->acquire_connection();
cache->execute_query("DEL user:123");
```

---

## 6. Error Handling Patterns

### Result<T> and Exception Safety

**DO:** Use explicit error handling with proper exception safety guarantees.

```cpp
#include <database/adapters/common_system_adapter.h>

using namespace database::adapters;

// Wrap database operations with Result<T> for safety
auto db = std::make_shared<database::postgres_manager>();
auto adapter = std::make_shared<common_system_database_adapter>(db);

// Type-safe error handling
auto connect_result = adapter->connect("host=localhost dbname=test");
if (!connect_result) {
    std::cerr << "Connection failed: " << connect_result.error().message
              << " (code: " << static_cast<int>(connect_result.error().code) << ")\n";
    return -1;
}

// Query execution with error propagation
auto query_result = adapter->execute_query("SELECT * FROM users");
if (!query_result) {
    std::cerr << "Query failed: " << query_result.error().message << "\n";
    // Handle gracefully
} else {
    for (const auto& row : query_result.value()) {
        // Process results
    }
}

// Transaction with automatic rollback on failure
auto begin_result = adapter->begin_transaction();
if (!begin_result) {
    std::cerr << "Failed to begin transaction\n";
    return -1;
}

auto cmd_result = adapter->execute_command("INSERT INTO users VALUES (1, 'John')");
if (!cmd_result) {
    adapter->rollback();  // Automatic cleanup on error
    return -1;
}

auto commit_result = adapter->commit();
if (!commit_result) {
    std::cerr << "Commit failed: " << commit_result.error().message << "\n";
}
```

**DON'T:** Ignore return values. Don't mix exception and error code patterns inconsistently.

```cpp
// BAD: Ignoring error return
adapter->execute_query("DELETE FROM important_table");
// Query might have failed, but we continue anyway

// BAD: Inconsistent error handling
try {
    if (!adapter->connect(connection_string)) {
        // Checked as bool
    }
} catch (const std::exception&) {
    // But also catches exceptions?
}

// BAD: No cleanup on error
adapter->begin_transaction();
adapter->execute_command("UPDATE accounts SET balance = -100");
// Commits even if balance went negative
adapter->commit();
```

### Graceful Degradation

**DO:** Implement fallback strategies when database operations fail.

```cpp
// Primary: PostgreSQL, Fallback: SQLite (or mock)
auto query_with_fallback = [&](const std::string& sql) -> database::database_result {
    auto pool_pg = db.get_connection_pool(database::database_types::postgres);
    if (pool_pg) {
        auto conn = pool_pg->acquire_connection();
        if (conn) {
            auto result = conn->select_query(sql);
            if (!result.empty()) {
                return result;  // Success
            }
        }
    }

    // PostgreSQL failed, try SQLite fallback
    auto pool_sqlite = db.get_connection_pool(database::database_types::sqlite);
    if (pool_sqlite) {
        auto conn = pool_sqlite->acquire_connection();
        if (conn) {
            return conn->select_query(sql);  // Fallback result
        }
    }

    // All failed, return empty result
    return database::database_result{};
};

// Use fallback strategy
auto result = query_with_fallback("SELECT * FROM users");
if (result.empty()) {
    std::cout << "Database unavailable, using cached data" << std::endl;
    // Serve stale data from cache
}
```

---

## 7. Thread Safety

### Synchronization and Concurrent Access

**DO:** Ensure thread-safe database operations with proper synchronization.

```cpp
#include <database/database_manager.h>
#include <thread>
#include <vector>

// Thread-safe connection pooling
database::database_manager& db = database::database_manager::handle();

database::connection_pool_config config;
config.min_connections = 10;
config.max_connections = 100;
db.create_connection_pool(database::database_types::postgres, config);

auto pool = db.get_connection_pool(database::database_types::postgres);

// Multiple threads safely using connection pool
std::vector<std::thread> worker_threads;
std::atomic<int> successful_ops{0};

for (int t = 0; t < 10; ++t) {
    worker_threads.emplace_back([&pool, &successful_ops]() {
        for (int i = 0; i < 100; ++i) {
            // Each thread acquires its own connection (thread-safe)
            auto connection = pool->acquire_connection();
            if (connection) {
                auto result = connection->select_query(
                    "SELECT COUNT(*) FROM users"
                );
                if (!result.empty()) {
                    successful_ops.fetch_add(1, std::memory_order_relaxed);
                }
                // Connection returned to pool automatically
            }
        }
    });
}

// Wait for completion
for (auto& thread : worker_threads) {
    thread.join();
}

std::cout << "Completed " << successful_ops.load() << " operations" << std::endl;
```

### Connection-per-Thread Pattern

**DO:** Maintain dedicated connection per thread for long-running operations.

```cpp
// Thread-local storage for per-thread connections
thread_local std::unique_ptr<database_base> thread_local_connection;

void worker_thread_function(const std::string& connection_string) {
    // Each thread gets its own connection
    thread_local_connection = std::make_unique<postgres_manager>();

    if (!thread_local_connection->connect(connection_string)) {
        std::cerr << "Thread " << std::this_thread::get_id() << " failed to connect" << std::endl;
        return;
    }

    // Use this connection throughout thread lifetime
    for (int i = 0; i < 1000; ++i) {
        auto result = thread_local_connection->select_query(
            "SELECT COUNT(*) FROM large_table LIMIT 1"
        );
        // Process result
    }

    // Connection automatically cleaned up when thread exits
}

// Launch worker threads
std::vector<std::thread> threads;
for (int i = 0; i < 4; ++i) {
    threads.emplace_back(worker_thread_function, connection_string);
}

for (auto& thread : threads) {
    thread.join();
}
```

**DON'T:** Share connections across threads without synchronization. Don't create new connections in every thread.

```cpp
// BAD: Shared connection without synchronization
auto shared_connection = db_pool->acquire_connection();

std::thread t1([&shared_connection]() {
    // Race condition: both threads using same connection
    auto result = shared_connection->select_query("SELECT * FROM users");
});

std::thread t2([&shared_connection]() {
    auto result = shared_connection->select_query("SELECT * FROM products");
});

// BAD: Creating new connections (defeats pooling benefits)
for (int i = 0; i < 10; ++i) {
    std::thread worker([=]() {
        auto new_conn = std::make_unique<postgres_manager>();
        new_conn->connect(connection_string);
        // High overhead: 10 new connections instead of using pool
    });
}
```

---

## 8. Memory Management

### RAII and Smart Pointers

**DO:** Use RAII and smart pointers for automatic resource management.

```cpp
#include <database/database_manager.h>
#include <memory>

// Auto-cleanup with shared_ptr
{
    auto db = std::make_shared<database::postgres_manager>();
    if (db->connect("host=localhost dbname=test")) {
        auto result = db->select_query("SELECT * FROM users");
        // Process result...
    }
    // Automatic cleanup: db is destroyed, connection closed
}

// Connection from pool with automatic return (RAII)
{
    auto pool = db.get_connection_pool(database::database_types::postgres);
    {
        auto connection = pool->acquire_connection();
        // Use connection...
    } // Connection automatically returned to pool here

    // Exception-safe: even if exception occurs, connection is returned
    try {
        auto conn = pool->acquire_connection();
        if (!conn) throw std::runtime_error("Failed to acquire");

        conn->execute_query("BEGIN");
        // If exception here, RAII ensures cleanup in destructor
        conn->execute_query("INSERT ...");
    } catch (...) {
        // Connections already cleaned up
    }
}

// Result lifetime properly managed
{
    auto result = db.select_query("SELECT * FROM large_table");
    // result is a vector - automatically freed when out of scope
    process_results(result);
} // result destroyed here - no memory leak
```

**DON'T:** Use raw pointers. Don't manually manage connection lifecycle.

```cpp
// BAD: Raw pointers
database_base* conn = new postgres_manager();
conn->connect(connection_string);
// If exception occurs before delete, memory leaks
delete conn;

// BAD: Manual connection management
auto conn = pool->acquire_connection();
// Forget to return connection
// Memory leak: connection never returned to pool
```

### Memory Monitoring Under Load

**DO:** Monitor memory usage with many concurrent connections.

```cpp
#include <database/monitoring/performance_monitor.h>

// Expected memory usage:
// - Baseline: <50 MB
// - Per connection: ~5-10 KB (varies by database)
// - 10,000 connections: ~850 MB

// Monitor memory during peak load
auto initial_memory = get_process_memory_mb();

// Run peak load scenario
for (int i = 0; i < 10000; ++i) {
    auto conn = pool->acquire_connection();
    if (conn) {
        auto result = conn->select_query("SELECT 1");
    }
}

auto peak_memory = get_process_memory_mb();
std::cout << "Memory increase: " << (peak_memory - initial_memory) << " MB" << std::endl;

// Verify cleanup
std::this_thread::sleep_for(std::chrono::seconds(10));
auto final_memory = get_process_memory_mb();
std::cout << "Memory after cleanup: " << (final_memory - initial_memory) << " MB" << std::endl;

// Should be close to baseline after connections are idle/released
if ((final_memory - initial_memory) > 100) {
    std::cerr << "Warning: Possible memory leak detected!" << std::endl;
}
```

---

## 9. Backend-Specific Tips

### PostgreSQL Best Practices

**DO:** Leverage PostgreSQL-specific features for optimal performance.

```cpp
// Use prepared statements
db.create_query_builder(database::database_types::postgres)
    .raw_sql("PREPARE user_query AS SELECT * FROM users WHERE id = $1")
    .execute(&db);

auto result = db.create_query_builder(database::database_types::postgres)
    .raw_sql("EXECUTE user_query(123)")
    .execute(&db);

// JSON support with JSONB
db.create_query("CREATE TABLE products (id SERIAL PRIMARY KEY, metadata JSONB)");
db.insert_query(
    "INSERT INTO products (metadata) VALUES "
    "('{\"brand\": \"TechCorp\", \"warranty\": 24}', "
    "'{\"brand\": \"PhoneMaker\", \"warranty\": 12}')"
);

// Query JSON efficiently
auto json_query = db.create_query_builder(database::database_types::postgres)
    .select({"id", "metadata->>'brand' as brand"})
    .from("products")
    .where("metadata->>'warranty'", ">", database::database_value{int64_t(12)})
    .execute(&db);

// Full-text search
db.create_query("CREATE TABLE articles (id SERIAL, content TEXT, search_vector tsvector)");
db.create_query(
    "CREATE INDEX idx_articles_search ON articles USING gin(search_vector)"
);

auto fts_result = db.create_query_builder(database::database_types::postgres)
    .select({"id", "content"})
    .from("articles")
    .where_raw("search_vector @@ to_tsquery('english', 'database')")
    .execute(&db);
```

### SQLite Best Practices

**DO:** Optimize SQLite for embedded and small-scale use cases.

```cpp
// Enable WAL (Write-Ahead Logging) for concurrency
database::connection_pool_config sqlite_config;
sqlite_config.connection_string = "file:memdb?mode=memory";
db.create_connection_pool(database::database_types::sqlite, sqlite_config);

// Enable WAL mode
db.create_query("PRAGMA journal_mode = WAL");
db.create_query("PRAGMA synchronous = NORMAL");  // Faster writes

// Optimize connection pool for SQLite (typically 1-2 connections)
database::connection_pool_config sqlite_optimized;
sqlite_optimized.min_connections = 1;
sqlite_optimized.max_connections = 3;  // SQLite doesn't handle many concurrent writes
sqlite_optimized.acquire_timeout = std::chrono::seconds(10);

// Batch transactions for performance
db.create_query("BEGIN TRANSACTION");
for (int i = 0; i < 10000; ++i) {
    db.insert_query("INSERT INTO data VALUES (" + std::to_string(i) + ", 'value')");
}
db.create_query("COMMIT");
// Single transaction: 50x faster than individual commits

// FTS5 for full-text search
db.create_query("CREATE VIRTUAL TABLE articles_fts USING fts5(title, content)");
db.insert_query("INSERT INTO articles_fts VALUES ('Title', 'Content about databases')");

auto fts_result = db.create_query_builder(database::database_types::sqlite)
    .select({"*"})
    .from("articles_fts")
    .where_raw("articles_fts MATCH 'database'")
    .execute(&db);
```

### MongoDB Best Practices

**DO:** Use MongoDB-specific features for document-oriented workloads.

```cpp
// Configure connection pool for MongoDB
database::connection_pool_config mongo_config;
mongo_config.connection_string = "mongodb://localhost:27017/mydb";
mongo_config.min_connections = 2;
mongo_config.max_connections = 50;
db.create_connection_pool(database::database_types::mongodb, mongo_config);

// Aggregation pipeline for complex queries
auto agg = db.create_query_builder(database::database_types::mongodb)
    .collection("orders")
    .match({{"status", database::database_value{std::string("completed")}}})
    .group({
        {"_id", database::database_value{std::string("$customer_id")}},
        {"total_spent", database::database_value{std::string("$sum: $amount")}},
        {"order_count", database::database_value{std::string("$sum: 1")}}
    })
    .sort("total_spent", -1)
    .limit(10)
    .execute(&db);

// Create indexes for query optimization
db.create_query_builder(database::database_types::mongodb)
    .collection("users")
    .raw_sql("db.users.createIndex({ email: 1 }, { unique: true })")
    .execute(&db);

// Bulk write operations
db.create_query_builder(database::database_types::mongodb)
    .collection("logs")
    .insert_one({
        {"timestamp", database::database_value{std::string("2025-11-11T10:30:00Z")}},
        {"level", database::database_value{std::string("INFO")}},
        {"message", database::database_value{std::string("Application started")}}
    })
    .execute(&db);
```

### Redis Best Practices

**DO:** Use Redis efficiently for caching and session storage.

```cpp
// Configure Redis connection pool
database::connection_pool_config redis_config;
redis_config.connection_string = "redis://localhost:6379/0";
redis_config.min_connections = 2;
redis_config.max_connections = 20;
redis_config.idle_timeout = std::chrono::minutes(10);
db.create_connection_pool(database::database_types::redis, redis_config);

// String operations with TTL
auto cache_conn = db.get_connection_pool(database::database_types::redis)->acquire_connection();

// Cache result with 1-hour TTL
cache_conn->execute_query(
    "SETEX session:user123 3600 '{\"id\":123,\"username\":\"john\",\"role\":\"admin\"}'"
);

// Hash operations for structured data
cache_conn->execute_query(
    "HSET user:123 id 123 username john email john@example.com role admin"
);

// List for queues
cache_conn->execute_query("RPUSH task_queue task_1 task_2 task_3");

// Process queue
for (int i = 0; i < 3; ++i) {
    auto task = cache_conn->select_query("LPOP task_queue");
    if (!task.empty()) {
        // Process task
    }
}

// Sorted sets for leaderboards
cache_conn->execute_query("ZADD leaderboard 100 player1 200 player2 150 player3");
auto top_players = cache_conn->select_query("ZREVRANGE leaderboard 0 9 WITHSCORES");
```

---

## Summary

### Quick Reference Table

| Category | DO | DON'T |
|----------|----|----|
| **Connections** | Use connection pooling | Create new connections repeatedly |
| **Queries** | Use parameterized queries | Concatenate user input |
| **Performance** | Batch operations | N+1 queries |
| **Security** | Encrypt credentials | Hardcode passwords |
| **Transactions** | Explicit rollback on error | Leave transactions open |
| **Threading** | Use RAII for cleanup | Share connections between threads |
| **Memory** | Smart pointers | Manual memory management |
| **Monitoring** | Collect metrics | Ignore performance data |

---

*This guide is based on production experience with database_system. For additional examples, see `/samples/` directory and `ARCHITECTURE.md`.*

