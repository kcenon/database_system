---
doc_id: "DBS-FEAT-002-POOLING-SECURITY"
doc_title: "Database System Features - Pooling, Security, and Monitoring"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "FEAT"
---

# Database System Features - Pooling, Security, and Monitoring

> **SSOT**: This document is a focused sub-document of **Database System Features**, covering connection pooling, resilient connections, enterprise security, performance monitoring, asynchronous operations, proxy mode, the unified database system, common_system integration, C++20 modules, and the technology stack.

**Last Updated**: 2026-02-08
**Version**: 0.4.0.0

---

## Table of Contents

- [Connection Pooling](#connection-pooling)
- [Resilient Connections](#resilient-connections)
- [Enterprise Security](#enterprise-security)
- [Performance Monitoring](#performance-monitoring)
- [Asynchronous Operations](#asynchronous-operations)
- [Proxy Mode](#proxy-mode)
- [Unified Database System](#unified-database-system)
- [common_system Integration](#common_system-integration)
- [C++20 Modules](#c20-modules)
- [Technology Stack](#technology-stack)

---

## Connection Pooling

**Status**: Well-Tested (v3)
**Implementation**: `connection_pool.h/cpp`

### Connection Pool v3 Features

**Performance Improvements**:
- **77ns latency**: 65x faster than v2 (5μs → 77ns)
- **1.16M+ ops/s**: Throughput with thread_system integration
- **7.7x performance**: Under high load with adaptive job queue
- **Priority scheduling**: Connection acquisition with QoS levels
- **Graceful shutdown**: Cancellation tokens for clean termination

**Architecture**:
```cpp
#include <database/connection_pool.h>

// Connection pool configuration
connection_pool_config config;
config.min_connections = 10;           // Minimum pool size
config.max_connections = 100;          // Maximum pool size
config.acquire_timeout = std::chrono::seconds(5);
config.idle_timeout = std::chrono::seconds(30);
config.health_check_interval = std::chrono::seconds(60);
config.enable_health_checks = true;
config.connection_string = "host=localhost port=5432 dbname=mydb";

// Create connection pool
auto& db = database_manager::handle();
db.create_connection_pool(database_types::postgres, config);

// Acquire connection (RAII-managed)
auto pool = db.get_connection_pool(database_types::postgres);
auto connection = pool->acquire_connection();

if (connection) {
    // Use connection for operations
    auto result = connection->select_query("SELECT * FROM users");

    // Connection automatically returned to pool when destroyed
}
```

### Priority-Based Acquisition

```cpp
// High-priority connection for critical operations
auto critical_conn = pool->acquire_connection(connection_priority::high);

// Normal priority (default)
auto normal_conn = pool->acquire_connection(connection_priority::normal);

// Low priority for background tasks
auto background_conn = pool->acquire_connection(connection_priority::low);
```

### Health Monitoring

```cpp
// Enable automatic health checks
config.enable_health_checks = true;
config.health_check_interval = std::chrono::seconds(30);

// Health check query
config.health_check_query = "SELECT 1";

// Get pool statistics
auto stats = pool->get_statistics();
std::cout << "Active connections: " << stats.active_connections << std::endl;
std::cout << "Available connections: " << stats.available_connections << std::endl;
std::cout << "Total created: " << stats.total_created << std::endl;
std::cout << "Total destroyed: " << stats.total_destroyed << std::endl;
std::cout << "Failed health checks: " << stats.failed_health_checks << std::endl;
std::cout << "Average acquisition time: " << stats.avg_acquisition_time.count() << "ns" << std::endl;

// Check pool health
if (pool->is_healthy()) {
    std::cout << "Pool is healthy" << std::endl;
}
```

### Thread-System Integration

```cpp
#include <thread_system/thread_pool.h>

// Use thread_system for async connection acquisition
auto thread_pool = std::make_shared<thread_system::thread_pool>(8);

// Submit connection acquisition task
auto future = thread_pool->submit([&pool]() {
    auto conn = pool->acquire_connection();
    if (conn) {
        return conn->select_query("SELECT * FROM large_table");
    }
    return database_result{};
});

// Do other work...

// Get result when ready
auto result = future.get();
```

### Graceful Shutdown

```cpp
// Create cancellation token
auto shutdown_token = std::make_shared<cancellation_token>();

// Register signal handler
std::signal(SIGTERM, [](int) {
    shutdown_token->cancel();
});

// Pool respects cancellation
pool->set_cancellation_token(shutdown_token);

// On shutdown, pool drains gracefully
pool->shutdown();  // Waits for active connections, rejects new requests
```

---

## Resilient Connections

**Status**: Well-Tested
**Implementation**: `resilient/resilient_connection.h`

### Automatic Reconnection

Production-grade reliability with exponential backoff:

```cpp
#include <database/resilient/resilient_connection.h>

// Configure resilient connection
resilient_connection_config resilient_config;
resilient_config.max_retries = 5;
resilient_config.initial_retry_delay = std::chrono::milliseconds(100);
resilient_config.max_retry_delay = std::chrono::seconds(30);
resilient_config.backoff_multiplier = 2.0;  // Exponential backoff
resilient_config.enable_jitter = true;  // Randomize retry delays
resilient_config.circuit_breaker_threshold = 10;  // Failures before circuit opens
resilient_config.circuit_breaker_timeout = std::chrono::seconds(60);

// Create resilient connection wrapper
auto base_connection = std::make_shared<postgres_manager>();
auto resilient_conn = std::make_shared<resilient_connection>(base_connection, resilient_config);

// Connect with automatic retry
auto connect_result = resilient_conn->connect("host=localhost port=5432 dbname=mydb");
// Automatically retries with exponential backoff on failure
// Recovery time: <1s for transient failures

// Queries automatically retry on connection loss
auto result = resilient_conn->select_query("SELECT * FROM users");
// If connection lost, automatically reconnects and retries query
```

### Health Monitoring

Real-time connection quality scoring:

```cpp
// Get connection health
auto health_score = resilient_conn->get_health_score();
std::cout << "Connection health: " << health_score << "/100" << std::endl;

// Health score based on:
// - Connection uptime
// - Query success rate
// - Response time
// - Error frequency

// Register health change callback
resilient_conn->on_health_change([](int old_score, int new_score) {
    std::cout << "Health changed: " << old_score << " -> " << new_score << std::endl;
    if (new_score < 50) {
        alert("Database connection degraded");
    }
});

// Get detailed health metrics
auto health_metrics = resilient_conn->get_health_metrics();
std::cout << "Uptime: " << health_metrics.uptime.count() << "s" << std::endl;
std::cout << "Total queries: " << health_metrics.total_queries << std::endl;
std::cout << "Failed queries: " << health_metrics.failed_queries << std::endl;
std::cout << "Success rate: " << (health_metrics.success_rate * 100) << "%" << std::endl;
std::cout << "Avg response time: " << health_metrics.avg_response_time.count() << "ms" << std::endl;
```

### Circuit Breaker Pattern

Prevents cascade failures:

```cpp
// Circuit breaker automatically opens after threshold failures
resilient_config.circuit_breaker_threshold = 5;
resilient_config.circuit_breaker_timeout = std::chrono::seconds(30);

// After 5 consecutive failures, circuit opens
// All requests immediately fail for 30 seconds
// Then circuit enters half-open state (tries one request)
// If successful, circuit closes; if failed, remains open

// Get circuit state
auto circuit_state = resilient_conn->get_circuit_state();
switch (circuit_state) {
    case circuit_state::closed:
        std::cout << "Circuit closed (normal operation)" << std::endl;
        break;
    case circuit_state::open:
        std::cout << "Circuit open (failing fast)" << std::endl;
        break;
    case circuit_state::half_open:
        std::cout << "Circuit half-open (testing recovery)" << std::endl;
        break;
}
```

---

## Enterprise Security

**Status**: Full Support
**Implementation**: `security/secure_connection.h`

The security module provides six dedicated components accessible via dependency injection through `database_context`.

### Credential Manager

Encrypted credential storage with master key management:

```cpp
#include <database/security/secure_connection.h>

auto context = std::make_shared<database_context>();
auto cred_mgr = context->get_credential_manager();

// Store encrypted credentials
security::security_credentials creds;
creds.username = "admin";
creds.password_hash = cred_mgr->hash_password("secure_pass");
creds.auth_method = security::authentication_method::password;
creds.encryption = security::encryption_type::tls;
cred_mgr->store_credentials("primary_db", creds);

// Retrieve credentials
auto stored = cred_mgr->get_credentials("primary_db");

// Key rotation
cred_mgr->set_master_key("new-master-key");
cred_mgr->rotate_encryption_keys();
```

**Supported Authentication Methods**:
- Password-based authentication
- Certificate-based authentication
- Kerberos
- OAuth2 (with client_id, client_secret, token management)
- JWT (with token expiry tracking)

### Connection Security

Secure database connections with TLS/SSL and mutual authentication:

```cpp
security::security_credentials creds;
creds.encryption = security::encryption_type::tls;
creds.verify_certificate = true;
creds.mutual_authentication = true;

security::connection_security conn_sec(creds);

// Configure TLS
conn_sec.configure_tls("client.crt", "client.key", "ca.crt");
conn_sec.set_cipher_suite("TLS_AES_256_GCM_SHA384");

// Establish secure connection
conn_sec.establish_secure_connection("db.example.com", 5432);

// Connection string encryption for secure storage
auto encrypted = conn_sec.encrypt_connection_string("host=localhost password=secret");
auto decrypted = conn_sec.decrypt_connection_string(encrypted);
```

### Query Security

SQL injection prevention and query analysis:

```cpp
// SQL injection detection
bool safe = security::query_security::is_query_safe(user_input);
std::string sanitized = security::query_security::sanitize_input(user_input);
std::string escaped = security::query_security::escape_sql_string(user_value);

// Suspicious pattern detection
bool suspicious = security::query_security::detect_suspicious_patterns(query);

// Table access validation
auto tables = security::query_security::extract_table_names(query);
bool allowed = security::query_security::validate_table_access("users", "SELECT", "admin");

// Convert to prepared statement
auto prepared = security::query_security::convert_to_prepared_statement(
    query, {database_value{std::string("param1")}, database_value{int64_t(42)}});
```

### Role-Based Access Control (RBAC)

Fine-grained permission management:

```cpp
auto access_ctrl = context->get_access_control();

// Create roles with permissions
security::access_control::role admin_role;
admin_role.name = "db_admin";
admin_role.permissions = {
    security::access_control::permission::select,
    security::access_control::permission::insert,
    security::access_control::permission::update,
    security::access_control::permission::admin
};
admin_role.allowed_tables = {"*"};
access_ctrl->create_role(admin_role);

// Assign roles and check permissions
access_ctrl->assign_role_to_user("user_123", "db_admin");
bool can_delete = access_ctrl->check_permission("user_123", "users", "DELETE");

// Session management
auto session_id = access_ctrl->create_session("user_123", "192.168.1.100");
bool valid = access_ctrl->validate_session(session_id);
access_ctrl->cleanup_expired_sessions();
```

### Security Audit Logger

Comprehensive security event logging and reporting:

```cpp
auto audit_log = context->get_audit_logger();

// Log database access events
audit_log->log_database_access("user_123", session_id, "SELECT", "users", query_hash, true);
audit_log->log_authentication_event("user_123", "192.168.1.100", true, "password");
audit_log->log_authorization_failure("user_456", "DROP", "users", "Insufficient privileges");

// Retrieve audit logs
auto recent_logs = audit_log->get_audit_logs(std::chrono::hours(24));
auto user_logs = audit_log->get_user_audit_logs("user_123", std::chrono::hours(168));

// Security reporting
auto report = audit_log->generate_security_report(std::chrono::hours(720));
auto suspicious = audit_log->detect_suspicious_activity(std::chrono::hours(24));

// Log management
audit_log->set_log_retention_period(std::chrono::hours(24 * 90));  // 90 days
audit_log->export_logs_to_file("audit_2026_Q1.log");
```

### Security Monitor

Real-time threat detection and alerting:

```cpp
auto sec_monitor = context->get_security_monitor();

// Register alert handler
sec_monitor->register_security_handler([](const security::security_monitor::security_alert& alert) {
    if (alert.level == security::security_monitor::threat_level::critical) {
        send_alert_notification(alert.description);
    }
});

// Active monitoring (called automatically by the system)
sec_monitor->analyze_query_patterns("user_123", query);
sec_monitor->detect_brute_force_attempts("192.168.1.100");
sec_monitor->monitor_privilege_escalation("user_456", "ALTER TABLE");

// Security metrics
auto failed_logins = sec_monitor->get_failed_login_count(std::chrono::hours(1));
auto suspicious_queries = sec_monitor->get_suspicious_query_count(std::chrono::hours(24));
double security_score = sec_monitor->calculate_security_score();
```

### Encryption Manager

Field-level and column-level data encryption:

```cpp
auto enc_mgr = context->get_encryption_manager();

// Master key management
enc_mgr->set_master_encryption_key("master-encryption-key-256bit");

// Column-level encryption configuration
enc_mgr->configure_encrypted_column("users", "ssn", security::encryption_type::aes256);
enc_mgr->configure_encrypted_column("users", "credit_card", security::encryption_type::aes256);

// Field data encryption/decryption
auto encrypted_ssn = enc_mgr->encrypt_field_data("123-45-6789", "ssn");
auto decrypted_ssn = enc_mgr->decrypt_field_data(encrypted_ssn, "ssn");

// Key rotation
enc_mgr->rotate_field_key("ssn");

// Check encryption status
bool is_encrypted = enc_mgr->is_column_encrypted("users", "ssn");
```

For production deployment details, see [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md).

---

## Performance Monitoring

**Status**: Full Support with monitoring_system integration
**Implementation**: `monitoring/`, see [BENCHMARKS.md](BENCHMARKS.md) for metrics

---

## Asynchronous Operations

**Status**: Full Support (C++20 coroutines optional, C++17 std::future fallback)
**Implementation**: `async/async_operations.h`

### C++20 Coroutines

```cpp
#include <database/async/async_operations.h>

// Async query with coroutines
database_awaitable<database_result> fetch_users_async() {
    auto db = co_await async_db_connect("host=localhost dbname=mydb");

    auto result = co_await db.execute_query_async(
        "SELECT * FROM users WHERE is_active = true"
    );

    co_return result;
}

// Use in async context
auto users = co_await fetch_users_async();
```

### C++20 Concepts Integration

The async operations now leverage C++20 concepts for compile-time type validation:

**Header**: `#include <database/core/concepts.h>`

**Available Concepts**:

| Concept | Description | Use Case |
|---------|-------------|----------|
| `SubmittableTask<F, Args...>` | Task callable for async executor | `async_executor.submit()` |
| `VoidCallable<F, Args...>` | Callback returning void | Completion handlers |
| `ErrorHandler<F>` | Exception handler callable | `on_error()` callbacks |
| `QueryCallback<F, ResultType>` | Query result handler | `on_query_complete()` |
| `StreamEventHandler<F, EventType>` | Stream event processor | Real-time data handlers |
| `StreamEventFilter<F, EventType>` | Event filtering predicate | Event filtering |
| `TransactionAction<F>` | Saga forward action | Distributed transactions |
| `CompensationAction<F>` | Saga rollback action | Compensation logic |

**Type-Safe Async Task Submission**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept-constrained task submission
template<SubmittableTask<database_result> F>
auto submit_query_task(async_executor& executor, F&& func) {
    return executor.submit(std::forward<F>(func));
}

// Usage - compiler validates callable signature at compile time
auto future = submit_query_task(executor, [&db]() {
    return db.select_query("SELECT * FROM users");
});
```

**Type-Safe Error Handling**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept-constrained error handler registration
template<ErrorHandler F>
void set_error_handler(F&& handler) {
    error_handler_ = std::forward<F>(handler);
}

// Usage - compiler validates exception handler signature
set_error_handler([](const std::exception& e) {
    log_error("Database error: " + std::string(e.what()));
});
```

**Saga Pattern with Concepts**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Add saga step with concept constraints
template<TransactionAction A, CompensationAction C>
void add_saga_step(A&& action, C&& compensation) {
    steps_.emplace_back(
        std::forward<A>(action),
        std::forward<C>(compensation)
    );
}

// Usage
saga_builder.add_step(
    []() { /* Create order */ },
    []() { /* Cancel order */ }
);
```

**Benefits**:
- **Clearer error messages**: Template errors shown as concept violations
- **Self-documenting code**: Type requirements expressed explicitly
- **Better IDE support**: Improved auto-completion and type hints
- **Backward compatible**: Legacy `std::function` overloads maintained

### C++17 Future-Based Async

```cpp
// Future-based async operations (C++17 fallback)
auto future_result = async_db.execute_async([&db]() {
    return db.select_query("SELECT * FROM large_table");
});

// Do other work...

// Get result when ready
auto result = future_result.get();
```

---

## Proxy Mode

**Status**: Full Support (Phase 4.1)
**Implementation**: `proxy/proxy_config.h`, `proxy/proxy_connector.h`

Proxy mode allows database_system clients to connect through a database_server middleware instead of directly to the database. This enables centralized connection management, security enforcement, and load balancing.

### Configuration

```cpp
#include <database/proxy/proxy_config.h>

database::proxy::proxy_connection_config config;
config.server_host = "db-gateway.internal";
config.server_port = 9432;
config.auth_token = "client-token-xyz";
config.connection_timeout = std::chrono::milliseconds{5000};
config.query_timeout = std::chrono::milliseconds{30000};
config.retry_count = 3;
config.retry_delay = std::chrono::milliseconds{1000};
config.use_tls = true;
config.ca_cert_path = "/etc/ssl/certs/ca.pem";

// Optional: mutual TLS (mTLS)
config.client_cert_path = "/etc/ssl/client.crt";
config.client_key_path = "/etc/ssl/client.key";

// Validate configuration
if (config.is_valid()) {
    // Use proxy connection
}
```

### Connection Modes

The system supports two connection modes:
- **Direct mode** (default): Direct connection to the database server
- **Proxy mode**: Connection through database_server middleware

```cpp
// Set connection mode to proxy
manager->set_connection_mode(connection_mode::proxy);
manager->configure_proxy(config);
```

---

## Unified Database System

**Status**: Full Support (Phase 6)
**Implementation**: `integrated/unified_database_system.h`, `integrated/core/database_coordinator.h`

The unified database system provides a zero-configuration entry point that integrates all adapters (logger, monitoring, thread) behind the scenes.

### Zero-Configuration Usage

```cpp
#include <database/integrated/unified_database_system.h>

using namespace database::integrated;

// Simplest usage - smart defaults
unified_database_system db;
auto result = db.connect("postgresql://localhost/mydb");
if (result.is_ok()) {
    auto rows = db.execute("SELECT * FROM users WHERE id = $1", 42);
}
```

### Builder Pattern Configuration

```cpp
auto db = unified_database_system::builder()
    .set_backend(backend_type::postgresql)
    .set_connection_string("host=localhost dbname=mydb")
    .set_pool_size(10, 50)
    .enable_logging(db_log_level::debug, "./logs")
    .enable_monitoring(true)
    .enable_async(4)  // 4 worker threads
    .build();

// Async query execution
auto future = db->execute_async("SELECT * FROM large_table");
// Do other work...
auto result = future.get();

// Transaction management
auto tx = db->begin_transaction();
tx->execute("INSERT INTO users (name) VALUES ($1)", "Alice");
tx->execute("UPDATE accounts SET balance = balance - 100");
tx->commit();
```

### Integrated Adapters

The coordinator integrates the following adapter backends:
- **Logger adapter**: Structured logging with configurable backends (system logger, null logger, fallback)
- **Monitoring adapter**: Performance metrics collection with optional monitoring_system integration
- **Thread adapter**: Async operation support with optional thread_system integration

---

## common_system Integration

**Status**: Full Support
**Implementation**: `include/kcenon/database/adapters/common_system_database_adapter.h`, `include/kcenon/database/di/service_registration.h`

When built with common_system (via `KCENON_HAS_COMMON_SYSTEM` feature flag), database_system provides adapter and DI integration.

### IDatabase Adapter

Bridges common_system's `IDatabase` interface with database_system's `database_manager`:

```cpp
#include <kcenon/database/adapters/common_system_database_adapter.h>

using namespace kcenon::database::adapters;

// Create adapter with specific database type
auto adapter = std::make_shared<common_system_database_adapter>(
    ::database::database_types::postgresql);

// Use through common_system IDatabase interface
auto connect_result = adapter->connect("host=localhost dbname=mydb");
auto query_result = adapter->execute_query("SELECT * FROM users");
auto cmd_result = adapter->execute_command("INSERT INTO logs VALUES (...)");

// Transaction support
adapter->begin_transaction();
adapter->execute_command("UPDATE accounts SET balance = balance - 100");
adapter->commit();

// Access underlying database_manager for advanced features
auto manager = adapter->get_manager();
```

### Service Container Registration

Register database services with common_system's dependency injection container:

```cpp
#include <kcenon/database/di/service_registration.h>

using namespace kcenon::database::di;

auto& container = common::di::service_container::global();

// Register with default configuration (PostgreSQL, singleton)
auto result = register_database_services(container);

// Or with custom configuration
database_registration_config config;
config.db_type = ::database::database_types::sqlite;
config.connection_string = "database.db";
config.connect_on_register = true;
config.lifetime = common::di::service_lifetime::singleton;
auto result = register_database_services(container, config);

// Resolve database anywhere in the application
auto db = container.resolve<common::interfaces::IDatabase>().value();
db->connect("host=localhost dbname=mydb");
auto query_result = db->execute_query("SELECT * FROM users");
```

### Feature Flags

Build configuration is controlled via unified feature flags:

```cpp
#include <kcenon/database/config/feature_flags.h>

#if KCENON_HAS_COMMON_SYSTEM
    // Use common_system Result<T>, IDatabase, DI container
#else
    // Use local fallbacks
#endif
```

---

## C++20 Modules

**Status**: Full Support
**Implementation**: `src/modules/database.cppm`

The database_system can be consumed as a C++20 module for faster compilation and better encapsulation.

### Module Structure

| Module | Partition | Contents |
|--------|-----------|----------|
| `kcenon.database` | (primary) | Aggregates all partitions |
| `kcenon.database:core` | Core | Types, context, manager, backend registry, proxy config |
| `kcenon.database:query` | Query | Query builder, conditions, dialects (SQL, MongoDB, Redis) |
| `kcenon.database:backends` | Backends | PostgreSQL, SQLite, MongoDB, Redis backends |

### Usage

```cpp
import kcenon.database;

using namespace database;

// Create database context and manager
auto context = std::make_shared<database_context>();
auto manager = std::make_shared<database_manager>(context);

// Configure and connect
manager->set_mode(database_types::postgres);
auto result = manager->connect_result("host=localhost dbname=test");
if (result.is_ok()) {
    auto query_result = manager->select_query_result("SELECT * FROM users");
}

// Use query builder
auto builder = manager->create_query_builder();
auto query = builder
    .select({"id", "name"})
    .from("users")
    .where("active", "=", true)
    .limit(10)
    .build();
```

### Dependencies

```
kcenon.database
  ├── kcenon.common (Tier 0) - Result<T>, error handling
  ├── kcenon.thread (Tier 1) - Thread pool for async operations (optional)
  └── kcenon.container (Tier 1) - Serialization (optional)
```

---

## Technology Stack

### Dependencies

**Required**:
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+

**Optional** (for specific backends):
- libpqxx (PostgreSQL)
- sqlite3 (SQLite)
- mongo-cxx-driver (MongoDB)
- hiredis (Redis)

**Optional** (for enhanced features):
- thread_system (connection pooling v3, async operations)
- monitoring_system (performance metrics, Prometheus export)
- logger_system (structured logging)

### CMake Integration

See [Project Structure](PROJECT_STRUCTURE.md) for build configuration.

---

## Related Documents

- [FEATURES.md](FEATURES.md) - Features index
- [FEATURES_BACKENDS.md](FEATURES_BACKENDS.md) - PostgreSQL, SQLite, MongoDB, Redis backend docs
- [FEATURES_ORM_QUERY.md](FEATURES_ORM_QUERY.md) - ORM framework and query builders
- [BENCHMARKS.md](BENCHMARKS.md) - Performance benchmarks
- [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md) - Production quality details
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) - Project organization

---

**Last Updated**: 2026-02-08
