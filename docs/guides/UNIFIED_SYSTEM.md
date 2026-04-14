---
doc_id: "DBS-GUID-020"
doc_title: "Unified Database System Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Unified Database System Guide

> **SSOT**: This document is the single source of truth for **Unified Database System Guide**.

> **Language:** **English** | 한국어 <!-- TODO: UNIFIED_SYSTEM.kr.md translation -->

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
  - [System Layers](#system-layers)
  - [Component Relationships](#component-relationships)
- [Configuration](#configuration)
  - [unified\_db\_config](#unified_db_config)
  - [Database Configuration](#database-configuration)
  - [Connection Pool Configuration](#connection-pool-configuration)
  - [Logger Configuration](#logger-configuration)
  - [Monitoring Configuration](#monitoring-configuration)
  - [Thread Pool Configuration](#thread-pool-configuration)
  - [Integration Flags](#integration-flags)
- [Connection String Builder](#connection-string-builder)
  - [Supported Backends](#supported-backends)
  - [Builder API](#builder-api)
  - [SSL Modes](#ssl-modes)
- [Database Coordinator](#database-coordinator)
  - [Initialization Sequence](#initialization-sequence)
  - [Shutdown Sequence](#shutdown-sequence)
  - [Health Checks](#health-checks)
  - [Coordinator Statistics](#coordinator-statistics)
- [Integration Adapters](#integration-adapters)
  - [Logger Adapter](#logger-adapter)
  - [Monitoring Adapter](#monitoring-adapter)
  - [Thread Adapter](#thread-adapter)
  - [Backend Selection Pattern](#backend-selection-pattern)
- [unified\_database\_system API](#unified_database_system-api)
  - [Builder Pattern Setup](#builder-pattern-setup)
  - [Synchronous Operations](#synchronous-operations)
  - [Asynchronous Operations](#asynchronous-operations)
  - [Transactions](#transactions)
  - [Metrics and Health](#metrics-and-health)
- [Protocol Layer](#protocol-layer)
  - [Message Format](#message-format)
  - [Message Types](#message-types)
  - [Protocol Serializer](#protocol-serializer)
  - [Container System Integration](#container-system-integration)
- [Examples](#examples)
  - [Example 1: Zero-Config Quick Start](#example-1-zero-config-quick-start)
  - [Example 2: Production Configuration](#example-2-production-configuration)
  - [Example 3: Async Operations with Monitoring](#example-3-async-operations-with-monitoring)
  - [Example 4: Connection String Builder Usage](#example-4-connection-string-builder-usage)
- [Configuration Defaults Reference](#configuration-defaults-reference)
- [Limitations and Known Issues](#limitations-and-known-issues)
- [Related Documentation](#related-documentation)

---

## Overview

The **unified database system** (`database/integrated/`) is the top-level integration layer that combines all database subsystems into a single, easy-to-use API. It provides:

- **Zero-config startup** with sensible defaults
- **Builder-pattern configuration** for full customization
- **Coordinated lifecycle management** for adapters (logging, monitoring, threading)
- **Binary protocol** for client-server communication
- **Connection string builder** for type-safe connection string construction

**Source files:**

| File | Purpose |
|------|---------|
| `database/integrated/unified_database_system.h` | Main API — builder, sync/async operations, transactions |
| `database/integrated/unified_database_system.cpp` | PIMPL implementation with EMA-based latency tracking |
| `database/integrated/core/configuration.h` | All configuration structs and enums |
| `database/integrated/core/database_coordinator.h` | Adapter lifecycle manager |
| `database/integrated/core/database_coordinator.cpp` | Coordinator PIMPL implementation |
| `database/integrated/connection_string_builder.h` | Fluent connection string builder |
| `database/integrated/adapters/logger_adapter.h` | Logging adapter with backend selection |
| `database/integrated/adapters/monitoring_adapter.h` | Monitoring adapter with metrics |
| `database/integrated/adapters/thread_adapter.h` | Thread pool adapter for async ops |
| `database/protocol/database_protocol.h` | Binary protocol definitions |
| `database/protocol/database_protocol_container.h` | Optional container_system serialization |

---

## Architecture

### System Layers

```
┌─────────────────────────────────────────────────────────────────┐
│                      Application Code                           │
├─────────────────────────────────────────────────────────────────┤
│               unified_database_system (Public API)              │
│         Builder Pattern  │  Sync/Async  │  Transactions         │
├─────────────────────────────────────────────────────────────────┤
│                  database_coordinator                           │
│         Lifecycle Management  │  Health Aggregation             │
├──────────────────┬──────────────────┬───────────────────────────┤
│  logger_adapter  │ monitoring_adapter│    thread_adapter        │
│  ┌────────────┐  │  ┌────────────┐  │  ┌────────────┐          │
│  │  system    │  │  │  fallback  │  │  │  fallback  │          │
│  │  fallback  │  │  │  null      │  │  │  null      │          │
│  │  null      │  │  └────────────┘  │  └────────────┘          │
│  └────────────┘  │                  │                           │
├──────────────────┴──────────────────┴───────────────────────────┤
│           connection_string_builder  │  connection_pool         │
├─────────────────────────────────────────────────────────────────┤
│           Database Backends (postgres, sqlite, mongodb, redis)  │
├─────────────────────────────────────────────────────────────────┤
│           Protocol Layer (binary message serialization)         │
└─────────────────────────────────────────────────────────────────┘
```

### Component Relationships

```
unified_db_config ─────────────────┐
    ├── database_config            │
    ├── pool_config                │
    ├── db_logger_config           │
    ├── db_monitoring_config       ├──► database_coordinator
    └── db_thread_config           │       ├── logger_adapter
                                   │       ├── monitoring_adapter
unified_database_system ───────────┘       └── thread_adapter
    ├── connect()                              ↑ (backend pattern)
    ├── execute() / select()                   ├── system backend
    ├── execute_async()                        ├── fallback backend
    ├── begin_transaction()                    └── null backend
    └── get_metrics() / check_health()
```

---

## Configuration

### unified_db_config

The top-level configuration struct combines all subsystem configurations into a single structure with a fluent builder API.

**Header:** `database/integrated/core/configuration.h`

```cpp
#include "database/integrated/core/configuration.h"

using namespace database::integrated;

auto config = unified_db_config{}
    .set_backend(backend_type::postgres, "host=localhost dbname=mydb")
    .set_credentials("admin", "password123")
    .set_pool_size(5, 20)
    .set_log_level(db_log_level::info)
    .enable_slow_query_logging(true, std::chrono::milliseconds(500))
    .enable_monitoring(true)
    .enable_prometheus(true, 9090)
    .set_thread_count(4);
```

**Builder methods:**

| Method | Parameters | Description |
|--------|------------|-------------|
| `set_backend()` | `backend_type, connection_string` | Set database type and connection string |
| `set_credentials()` | `username, password` | Set database credentials |
| `set_pool_size()` | `min, max` | Configure connection pool size |
| `set_pool_name()` | `name` | Set pool identifier for logging |
| `set_log_level()` | `db_log_level` | Set minimum logging level |
| `enable_query_logging()` | `bool` | Enable/disable SQL query logging |
| `enable_slow_query_logging()` | `bool, milliseconds` | Configure slow query detection |
| `enable_file_logging()` | `bool, directory` | Enable logging to files |
| `enable_monitoring()` | `bool` | Enable metrics and health checks |
| `enable_prometheus()` | `bool, port, endpoint` | Configure Prometheus export |
| `set_thread_count()` | `size_t` | Set worker threads (0 = auto-detect) |
| `enable_priority_scheduling()` | `bool` | Enable priority-based task/connection scheduling |
| `enable_ssl()` | `bool, cert_path, key_path` | Enable SSL/TLS for connections |
| `set_timeouts()` | `acquisition, idle` | Set connection pool timeouts |

### Database Configuration

`database_config` contains connection details and backend-specific settings.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `type` | `backend_type` | `postgres` | Database backend type |
| `connection_string` | `string` | `"host=localhost port=5432 dbname=postgres"` | Backend-specific connection string |
| `enable_ssl` | `bool` | `false` | Enable SSL/TLS |
| `ssl_cert_path` | `string` | `""` | Path to SSL certificate |
| `ssl_key_path` | `string` | `""` | Path to SSL key |
| `enable_prepared_statements` | `bool` | `true` | Enable prepared statement caching |
| `enable_query_cache` | `bool` | `false` | Enable query result caching |
| `query_cache_size` | `size_t` | `104857600` (100 MB) | Maximum query cache size in bytes |
| `username` | `string` | `""` | Database username |
| `password` | `string` | `""` | Database password |

**backend_type enum:**

| Value | Description |
|-------|-------------|
| `postgres` | PostgreSQL database |
| `sqlite` | SQLite embedded database |
| `mongodb` | MongoDB NoSQL database |
| `redis` | Redis key-value store |

### Connection Pool Configuration

`pool_config` controls the behavior of the database connection pool.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `pool_name` | `string` | `"default_pool"` | Pool identifier for logging |
| `min_connections` | `size_t` | `2` | Minimum connections to maintain |
| `max_connections` | `size_t` | `10` | Maximum connections allowed |
| `connection_timeout` | `seconds` | `30` | Timeout for acquiring a connection |
| `idle_timeout` | `seconds` | `300` (5 min) | Idle connection close timeout |
| `enable_health_checks` | `bool` | `true` | Enable periodic health checks |
| `health_check_interval` | `seconds` | `60` | Health check interval |
| `enable_priority_queue` | `bool` | `false` | Priority-based connection acquisition |

### Logger Configuration

`db_logger_config` controls database operation logging.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enable_query_logging` | `bool` | `false` | Log all SQL queries |
| `enable_connection_logging` | `bool` | `true` | Log connection pool events |
| `log_slow_queries` | `bool` | `true` | Auto-detect and log slow queries |
| `slow_query_threshold` | `milliseconds` | `1000` (1s) | Slow query threshold |
| `min_log_level` | `db_log_level` | `info` | Minimum log level |
| `enable_file_logging` | `bool` | `false` | Enable logging to file |
| `log_directory` | `string` | `"./logs"` | Log file directory |
| `log_rotation_size` | `size_t` | `10485760` (10 MB) | Log file rotation size |
| `log_rotation_count` | `size_t` | `5` | Rotated log files to keep |

**db_log_level enum** (ordered from most to least verbose):

| Level | Description |
|-------|-------------|
| `trace` | Most verbose — includes all operations |
| `debug` | Debug information for development |
| `info` | Informational messages (default) |
| `warning` | Warning conditions |
| `error` | Error conditions |
| `critical` | Critical failures requiring immediate attention |
| `fatal` | Fatal errors causing system shutdown |

### Monitoring Configuration

`db_monitoring_config` enables performance metrics collection and health monitoring.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `enable_metrics` | `bool` | `true` | Enable metrics collection |
| `enable_profiling` | `bool` | `false` | Enable performance profiling |
| `enable_health_checks` | `bool` | `true` | Enable health check endpoints |
| `metrics_interval` | `seconds` | `60` | Metrics collection interval |
| `connection_usage_warning_threshold` | `double` | `0.8` (80%) | Pool usage warning threshold |
| `query_latency_warning` | `milliseconds` | `500` | Query latency warning threshold |
| `enable_prometheus_export` | `bool` | `false` | Enable Prometheus endpoint |
| `prometheus_endpoint` | `string` | `"/metrics"` | Prometheus scraping endpoint |
| `prometheus_port` | `uint16_t` | `9090` | Prometheus server port |

### Thread Pool Configuration

`db_thread_config` configures the thread pool for async query execution.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `pool_name` | `string` | `"db_thread_pool"` | Thread pool identifier |
| `thread_count` | `size_t` | `0` (auto-detect) | Number of worker threads |
| `max_queue_size` | `size_t` | `1000` | Maximum queued tasks (0 = unlimited) |
| `enable_priority_scheduling` | `bool` | `false` | Priority-based task scheduling |
| `pool_type` | `thread_pool_type` | `standard` | Thread pool implementation type |

**thread_pool_type enum:**

| Value | Description |
|-------|-------------|
| `standard` | Standard thread pool (default) |
| `typed` | Typed thread pool with priority support |

### Integration Flags

`unified_db_config` includes flags for compile-time integration with external systems:

| Flag | Default | Description |
|------|---------|-------------|
| `enable_common_system_integration` | `true` | Enable common_system Result pattern, ILogger, LOG_* macros |
| `enable_thread_system_integration` | `true` | Enable thread_system typed thread pools |
| `enable_monitoring_system_integration` | `true` | Enable monitoring_system integration |

---

## Connection String Builder

The `connection_string_builder` provides a type-safe, fluent API for constructing backend-specific connection strings.

**Header:** `database/integrated/connection_string_builder.h`

### Supported Backends

| Backend | Required Fields | Output Format |
|---------|-----------------|---------------|
| PostgreSQL | host (recommended) | `host=... port=... dbname=... user=... password=...` |
| SQLite | database (or `in_memory()`) | `path/to/db.sqlite` or `:memory:` |
| MongoDB | host | MongoDB URI format |
| Redis | host | Redis connection format |

### Builder API

```cpp
#include "database/integrated/connection_string_builder.h"

using namespace database::integrated;

// PostgreSQL
auto pg = connection_string_builder()
    .host("db.example.com")
    .port(5432)
    .database("mydb")
    .user("admin")
    .password("secret")
    .ssl_mode(ssl_mode::require)
    .connect_timeout(10)
    .application_name("my-app")
    .build(backend_type::postgres);

if (pg.is_ok()) {
    std::cout << pg.value(); // "host=db.example.com port=5432 dbname=mydb ..."
}

// SQLite in-memory
auto sqlite = connection_string_builder()
    .in_memory()
    .build(backend_type::sqlite);
// Result: ":memory:"

// Custom options
auto custom = connection_string_builder()
    .host("localhost")
    .database("mydb")
    .option("connect_timeout", "10")
    .option("application_name", "test")
    .build(backend_type::postgres);
```

**Builder methods:**

| Method | Description |
|--------|-------------|
| `host(string_view)` | Set hostname or IP address |
| `port(uint16_t)` | Set port number (1–65535) |
| `database(string_view)` | Set database name or file path |
| `user(string_view)` | Set authentication username |
| `password(string_view)` | Set authentication password |
| `ssl_mode(ssl_mode)` | Set SSL connection mode |
| `connect_timeout(uint32_t)` | Set timeout in seconds |
| `application_name(string_view)` | Set application identifier |
| `in_memory()` | Configure SQLite in-memory mode |
| `option(key, value)` | Add a custom option |
| `build(backend_type)` | Build and validate the connection string |
| `reset()` | Reset builder to initial state |

### SSL Modes

| Mode | Description |
|------|-------------|
| `disable` | No SSL |
| `allow` | Try SSL, fall back to non-SSL |
| `prefer` | Try SSL first, fall back to non-SSL |
| `require` | Require SSL, no verification |
| `verify_ca` | Require SSL with CA verification |
| `verify_full` | Require SSL with full verification |

---

## Database Coordinator

The `database_coordinator` manages the lifecycle and coordination of all integration adapters.

**Header:** `database/integrated/core/database_coordinator.h`

### Initialization Sequence

The coordinator initializes adapters in a specific order that respects dependencies:

```
Phase 1: Logger Adapter
    ↓ (for observability of subsequent initializations)
Phase 2: Monitoring Adapter
    ↓ (for tracking initialization metrics)
Phase 3: Thread Adapter
    ↓ (for async operations)
[Phase 4: Connection Pool — planned for Phase 6]
```

If any phase fails, previously initialized adapters are shut down in reverse order (rollback):

```cpp
database_coordinator coordinator(config);
auto result = coordinator.initialize();
if (!result.is_ok()) {
    // All partially-initialized adapters have been cleaned up
    std::cerr << result.error().message << "\n";
}
```

### Shutdown Sequence

Shutdown proceeds in **reverse initialization order** to ensure clean teardown:

```
Phase 1: Thread Adapter (stop accepting new tasks, drain queue)
    ↓
Phase 2: Monitoring Adapter (flush final metrics)
    ↓
Phase 3: Logger Adapter (flush final logs, close files — last)
```

The logger is shut down last so it can log events from other adapter shutdowns. The destructor automatically calls `shutdown()` if still initialized.

### Health Checks

```cpp
auto health = coordinator.check_health();
if (health.is_ok() && health.value()) {
    // All adapters are healthy
}
```

The health check verifies:
- Logger adapter exists and is functional
- Monitoring adapter exists and passes its own health check
- Thread pool adapter exists

### Coordinator Statistics

```cpp
auto stats_result = coordinator.get_stats();
if (stats_result.is_ok()) {
    auto& stats = stats_result.value();
    // stats.is_initialized    — bool
    // stats.logger_healthy    — bool
    // stats.monitoring_healthy — bool
    // stats.thread_pool_healthy — bool
    // stats.uptime            — std::chrono::milliseconds
    // stats.init_time         — std::chrono::system_clock::time_point
}
```

---

## Integration Adapters

Each adapter follows the **Backend Pattern** for runtime polymorphism: the adapter holds a `std::unique_ptr<backend>` that can be swapped at construction time without conditional compilation.

### Logger Adapter

**Header:** `database/integrated/adapters/logger_adapter.h`

The logger adapter provides unified logging for all database operations with automatic query sanitization and slow query detection.

**Backend types (`logger_backend_type`):**

| Type | Description |
|------|-------------|
| `auto_select` | Automatically selects best available backend (default) |
| `system` | Uses common_system `ILogger` and `GlobalLoggerRegistry` |
| `fallback` | Uses `std::cout` + `std::ofstream` |
| `null` | No-op backend — discards all logs |

**Key methods:**

```cpp
logger_adapter logger(config.logger);
logger.initialize();

// SQL query logging (auto-sanitizes passwords, auto-detects slow queries)
logger.log_query(db_log_level::info,
    "SELECT * FROM users WHERE id = 123",
    std::chrono::microseconds(1500));

// Connection pool events
logger.log_connection_event("acquired", "Pool: main_pool, Priority: high");
logger.log_pool_event("resized", 15, 5); // 15 active, 5 idle

// Transaction logging
logger.log_transaction("commit", true, "isolation: read_committed");

// Error logging with SQL state codes
logger.log_error("execute_query", "relation does not exist", "42P01");

// Generic logging
logger.log(db_log_level::warning, "Connection pool usage above 80%");

logger.flush();
logger.shutdown();
```

**Features:**
- SQL query sanitization (password removal, truncation)
- Automatic slow query warning when duration exceeds threshold
- File logging with rotation (configurable size and count)
- Thread-safe — all public methods are thread-safe

### Monitoring Adapter

**Header:** `database/integrated/adapters/monitoring_adapter.h`

The monitoring adapter tracks connection pool utilization, query performance, and transaction lifecycle.

**Backend types (`monitoring_backend_type`):**

| Type | Description |
|------|-------------|
| `auto_select` | Automatically selects best available backend (default) |
| `system` | Uses monitoring_system (requires `HAVE_SYSTEM_MONITORING_BACKEND`) |
| `fallback` | Uses internal metrics storage |
| `null` | No-op backend — discards all metrics |

**Key methods:**

```cpp
monitoring_adapter monitor(config.monitoring);
monitor.initialize();

// Record operations
monitor.record_query_execution(std::chrono::microseconds(1500), true);
monitor.record_connection_acquired();
monitor.record_connection_released();
monitor.update_pool_stats(15, 5, 20); // active, idle, total

// Transaction tracking
monitor.record_transaction_begin();
monitor.record_transaction_commit();   // or record_transaction_rollback()

// Retrieve metrics
auto metrics = monitor.get_database_metrics();
if (metrics.is_ok()) {
    auto& m = metrics.value();
    // m.active_connections, m.total_queries, m.avg_query_latency
    // m.p95_query_latency, m.p99_query_latency
    // m.queries_per_second, m.transaction_commit_rate
}

// Prometheus export
std::string prom = monitor.export_prometheus_metrics();

// Health check
auto health = monitor.check_health();

monitor.reset();    // Reset all metrics
monitor.shutdown();
```

**database_metrics fields:**

| Category | Fields |
|----------|--------|
| Connection pool | `active_connections`, `idle_connections`, `total_connections`, `connection_usage_percent` |
| Query performance | `total_queries`, `successful_queries`, `failed_queries`, `query_success_rate` |
| Latency | `avg_query_latency`, `min_query_latency`, `max_query_latency`, `p95_query_latency`, `p99_query_latency` |
| Transactions | `active_transactions`, `committed_transactions`, `rolled_back_transactions`, `transaction_commit_rate` |
| Throughput | `queries_per_second`, `transactions_per_second` |

### Thread Adapter

**Header:** `database/integrated/adapters/thread_adapter.h`

The thread adapter provides async task execution using a configurable thread pool.

**Backend types (`thread_backend_type`):**

| Type | Description |
|------|-------------|
| `auto_select` | Automatically selects best available backend (default) |
| `fallback` | Uses `std::thread` pool |
| `null` | Synchronous execution — no threading |

**Key methods:**

```cpp
thread_adapter pool(config.thread);
pool.initialize();

// Fire-and-forget execution
pool.execute([]() {
    // background task
});

// Submit with future (uses C++20 SubmittableTask concept)
auto future = pool.submit([]() -> int {
    return execute_expensive_query();
});
int result = future.get();

// Wait for all tasks
pool.wait_for_completion();

// Wait with timeout
bool completed = pool.wait_for_completion_timeout(
    std::chrono::milliseconds(5000));

// Statistics
std::size_t workers = pool.worker_count();
std::size_t pending = pool.queue_size();
bool idle = pool.is_idle();

pool.shutdown();
```

The `submit()` method uses the `SubmittableTask` C++20 concept for compile-time validation:

```cpp
template <typename F, typename... Args>
    requires concepts::SubmittableTask<F, Args...>
auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;
```

### Backend Selection Pattern

All adapters follow the same pattern:

1. Constructor accepts a `*_backend_type` enum (default: `auto_select`)
2. `auto_select` probes for the best available backend at runtime
3. A `create_backend()` factory method instantiates the appropriate backend
4. The backend is stored as `std::unique_ptr<backend_interface>`
5. All adapter methods delegate to the backend

This eliminates conditional compilation (`#ifdef`) and allows backend selection to be deferred to runtime.

---

## unified_database_system API

**Header:** `database/integrated/unified_database_system.h`

### Builder Pattern Setup

The `unified_database_system` supports two construction approaches:

**1. Zero-config (default constructor):**

```cpp
#include "database/integrated/unified_database_system.h"

using namespace database::integrated;

// Zero-config with defaults (pool 2-10, info logging, monitoring enabled, 4 threads)
unified_database_system db;
auto result = db.connect(backend_type::postgres, "host=localhost dbname=mydb");
```

**2. Builder pattern (custom configuration):**

```cpp
auto db = unified_database_system::create_builder()
    .set_backend(backend_type::postgres)
    .set_connection_string("host=localhost dbname=mydb")
    .set_pool_size(5, 20)
    .enable_logging(db_log_level::info)
    .enable_monitoring(true)
    .enable_async(4)  // 4 worker threads
    .set_slow_query_threshold(std::chrono::milliseconds(500))
    .build();  // returns std::unique_ptr<unified_database_system>

auto result = db->connect("host=localhost dbname=mydb");
```

**3. Direct config construction:**

```cpp
auto config = unified_db_config{}
    .set_backend(backend_type::postgres, "host=localhost dbname=mydb")
    .set_pool_size(5, 20)
    .set_log_level(db_log_level::info)
    .enable_monitoring(true)
    .set_thread_count(4);

unified_database_system db(config);
auto result = db.connect("host=localhost dbname=mydb");
```

**Builder methods** (on `unified_database_system::builder`):

| Method | Parameters | Description |
|--------|------------|-------------|
| `set_backend()` | `backend_type` | Set database backend type |
| `set_connection_string()` | `string` | Set connection string |
| `set_pool_size()` | `min, max` | Configure connection pool size |
| `enable_logging()` | `db_log_level, log_dir` | Enable logging with level and directory |
| `enable_monitoring()` | `bool` | Enable metrics collection |
| `enable_async()` | `worker_threads` | Enable async with thread count |
| `set_slow_query_threshold()` | `milliseconds` | Set slow query detection threshold |
| `build()` | — | Build and return `unique_ptr<unified_database_system>` |

### Synchronous Operations

All synchronous methods return `Result<T>` for proper error handling:

```cpp
// Execute (general query) — returns Result<query_result>
auto result = db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");
if (result.is_ok()) {
    // result.value() is query_result
}

// Select — returns Result<query_result>
auto rows = db.select("SELECT * FROM users WHERE active = true");
if (rows.is_ok()) {
    // rows.value() contains column info and row data
}

// Insert — returns Result<size_t> (affected rows count)
auto inserted = db.insert("INSERT INTO logs (msg) VALUES ('started')");
if (inserted.is_ok()) {
    std::cout << "Inserted " << inserted.value() << " rows\n";
}

// Update — returns Result<size_t>
auto updated = db.update("UPDATE users SET active = false WHERE id = 5");

// Remove — returns Result<size_t>
auto removed = db.remove("DELETE FROM sessions WHERE expired = true");

// Parameterized queries
auto result = db.execute("SELECT * FROM users WHERE id = $1", {query_param{42}});
```

### Asynchronous Operations

Async methods return `std::future<Result<query_result>>`:

```cpp
// Async execute — returns future<Result<query_result>>
auto future = db.execute_async("INSERT INTO events (type) VALUES ('login')");
// ... do other work ...
auto result = future.get();
if (result.is_ok()) {
    // Success
}

// Async with priority (0=lowest, 100=highest)
auto high_priority = db.execute_async_priority(
    "SELECT * FROM critical_alerts", /*priority=*/1);
auto critical_result = high_priority.get();
```

### Transactions

```cpp
// Manual transaction — begin_transaction() returns Result<unique_ptr<transaction>>
auto tx_result = db.begin_transaction();
if (tx_result.is_ok()) {
    auto tx = std::move(tx_result.value());
    tx->execute("INSERT INTO orders (user_id, total) VALUES (1, 99.99)");
    tx->execute("UPDATE inventory SET quantity = quantity - 1 WHERE item_id = 42");
    tx->commit();
    // If tx goes out of scope without commit(), auto-rollback in destructor
}

// Batch transaction (list of queries)
auto result = db.execute_transaction({
    "INSERT INTO orders (user_id, total) VALUES (1, 99.99)",
    "UPDATE inventory SET quantity = quantity - 1 WHERE item_id = 42"
});

// Lambda-based transaction
auto result = db.in_transaction([](transaction& tx) {
    tx.execute("INSERT INTO orders (user_id, total) VALUES (1, 99.99)");
    tx.execute("UPDATE inventory SET quantity = quantity - 1 WHERE item_id = 42");
    return true;  // return value forwarded via Result
});
```

### Metrics and Health

```cpp
// Get performance metrics
auto metrics = db.get_metrics();
// metrics.total_queries, metrics.avg_latency, etc.

// Health check
auto health = db.check_health();
// health.is_healthy, health.details

// Reset metrics
db.reset_metrics();
```

---

## Protocol Layer

The protocol layer defines a binary message format for client-server communication between `unified_database_system` and remote database services.

**Header:** `database/protocol/database_protocol.h`

### Message Format

Every protocol message starts with a fixed 20-byte header:

```text
┌───────────┬───────────┬───────────┬──────────────────┬───────────────────┐
│  Magic    │  Version  │  Type     │  Request ID      │  Payload Size     │
│  (4 bytes)│  (2 bytes)│  (2 bytes)│  (8 bytes)       │  (4 bytes)        │
└───────────┴───────────┴───────────┴──────────────────┴───────────────────┘
```

| Field | Size | Value | Description |
|-------|------|-------|-------------|
| Magic | 4 bytes | `0xDB01DB01` | Protocol identifier |
| Version | 2 bytes | `1` | Protocol version |
| Type | 2 bytes | `message_type` enum | Message type identifier |
| Request ID | 8 bytes | variable | Unique request identifier for correlation |
| Payload Size | 4 bytes | variable | Payload size in bytes |

All multi-byte values use **little-endian** byte order.

### Message Types

| Category | Type | Value | Description |
|----------|------|-------|-------------|
| Connection | `CONNECT_REQUEST` | 1 | Client connection request |
| | `CONNECT_RESPONSE` | 2 | Server connection response |
| | `DISCONNECT` | 3 | Disconnect request |
| | `PING` | 4 | Keep-alive ping |
| | `PONG` | 5 | Keep-alive pong response |
| Query | `QUERY_REQUEST` | 10 | SQL query request |
| | `QUERY_RESPONSE` | 11 | Query result response |
| Transaction | `BEGIN_TRANSACTION` | 20 | Begin transaction |
| | `COMMIT_TRANSACTION` | 21 | Commit transaction |
| | `ROLLBACK_TRANSACTION` | 22 | Rollback transaction |
| | `TRANSACTION_RESPONSE` | 23 | Transaction result response |
| Prepared | `PREPARE_STATEMENT` | 30 | Prepare statement |
| | `EXECUTE_PREPARED` | 31 | Execute prepared statement |
| | `CLOSE_PREPARED` | 32 | Close prepared statement |
| Error | `ERROR_RESPONSE` | 100 | Error response |

### Protocol Serializer

The `protocol_serializer` class provides serialize/deserialize methods for all message types:

```cpp
#include "database/protocol/database_protocol.h"

using namespace database::protocol;

// Serialize a connect request
connect_request req;
req.database_type = "postgres";
req.connection_string = "host=localhost dbname=mydb";

auto bytes = protocol_serializer::serialize(req);

// Deserialize a query response
auto response = protocol_serializer::deserialize_query_response(bytes);
```

**Key message structures:**

| Structure | Key Fields |
|-----------|------------|
| `connect_request` | `database_type`, `connection_string` |
| `connect_response` | `success`, `session_id`, `error_message` |
| `query_request` | `query`, `session_id` |
| `query_response` | `success`, `columns`, `rows`, `affected_rows` |
| `transaction_request` | `session_id`, `operation` |
| `transaction_response` | `success`, `transaction_id` |
| `error_response` | `error_code`, `error_message`, `sql_state` |

### Container System Integration

When compiled with `USE_CONTAINER_SYSTEM`, the `container_protocol_serializer` provides additional serialization using the container_system library:

**Header:** `database/protocol/database_protocol_container.h`

```cpp
#ifdef USE_CONTAINER_SYSTEM
#include "database/protocol/database_protocol_container.h"

// Serialize to container format
auto container = container_protocol_serializer::serialize_container(request);

// Deserialize from container
auto query_req = container_protocol_serializer::deserialize_container_query_request(container);

// Serialize to JSON
std::string json = container_protocol_serializer::serialize_to_json(request);
#endif
```

---

## Examples

### Example 1: Zero-Config Quick Start

```cpp
#include "database/integrated/unified_database_system.h"

using namespace database::integrated;

int main() {
    // Zero-config: defaults (pool 2-10, info logging, monitoring, 4 threads)
    unified_database_system db;

    auto conn = db.connect(backend_type::postgres,
                           "host=localhost dbname=mydb");
    if (!conn.is_ok()) {
        std::cerr << "Connection failed: " << conn.error().message << "\n";
        return 1;
    }

    // Execute queries with default settings
    db.execute("CREATE TABLE IF NOT EXISTS users ("
               "id SERIAL PRIMARY KEY, "
               "name VARCHAR(100), "
               "email VARCHAR(255))");

    db.insert("INSERT INTO users (name, email) "
              "VALUES ('Alice', 'alice@example.com')");

    auto result = db.select("SELECT * FROM users");
    if (result.is_ok()) {
        // result.value() is query_result
    }

    return 0;
}
```

### Example 2: Production Configuration

```cpp
#include "database/integrated/unified_database_system.h"
#include "database/integrated/connection_string_builder.h"

using namespace database::integrated;

int main() {
    // Build connection string safely
    auto conn_result = connection_string_builder()
        .host("db.production.internal")
        .port(5432)
        .database("app_production")
        .user("app_service")
        .password(get_secret("DB_PASSWORD"))  // from secret manager
        .ssl_mode(ssl_mode::verify_full)
        .connect_timeout(10)
        .application_name("my-service")
        .build(backend_type::postgres);

    if (!conn_result.is_ok()) {
        std::cerr << "Invalid connection config: "
                  << conn_result.error().message << "\n";
        return 1;
    }

    // Full production configuration
    auto config = unified_db_config{}
        .set_backend(backend_type::postgres, conn_result.value())
        .set_pool_size(10, 50)
        .set_pool_name("app_production_pool")
        .set_log_level(db_log_level::warning)
        .enable_slow_query_logging(true, std::chrono::milliseconds(200))
        .enable_file_logging(true, "/var/log/myapp")
        .enable_monitoring(true)
        .enable_prometheus(true, 9090, "/metrics")
        .set_thread_count(8)
        .enable_priority_scheduling(true)
        .enable_ssl(true, "/etc/ssl/certs/db.crt", "/etc/ssl/private/db.key")
        .set_timeouts(std::chrono::seconds(10), std::chrono::seconds(120));

    // Initialize with coordinator for full lifecycle management
    database_coordinator coordinator(config);
    auto init = coordinator.initialize();
    if (!init.is_ok()) {
        std::cerr << "Init failed: " << init.error().message << "\n";
        return 1;
    }

    // Access adapters directly
    auto* logger = coordinator.get_logger();
    auto* monitor = coordinator.get_monitor();

    logger->log(db_log_level::info, "Production database system started");

    // ... application logic ...

    // Health check endpoint
    auto health = coordinator.check_health();
    auto stats = coordinator.get_stats();

    if (stats.is_ok()) {
        logger->log(db_log_level::info,
            "Uptime: " + std::to_string(stats.value().uptime.count()) + "ms");
    }

    // Prometheus metrics
    std::string prom_metrics = monitor->export_prometheus_metrics();

    coordinator.shutdown(); // or let destructor handle it
    return 0;
}
```

### Example 3: Async Operations with Monitoring

```cpp
#include "database/integrated/unified_database_system.h"

using namespace database::integrated;

int main() {
    auto db = unified_database_system::create_builder()
        .set_backend(backend_type::postgres)
        .set_connection_string("host=localhost dbname=mydb")
        .enable_monitoring(true)
        .enable_async(4)
        .build();

    auto conn = db->connect("host=localhost dbname=mydb");
    if (!conn.is_ok()) return 1;

    // Fire multiple async queries — returns future<Result<query_result>>
    std::vector<std::future<kcenon::common::Result<query_result>>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(
            db->execute_async(
                "INSERT INTO events (type, data) VALUES ('batch', '"
                + std::to_string(i) + "')"));
    }

    // Wait for all to complete
    int succeeded = 0;
    for (auto& f : futures) {
        auto result = f.get();
        if (result.is_ok()) ++succeeded;
    }

    std::cout << succeeded << "/100 inserts succeeded\n";

    // Check metrics after batch
    auto metrics = db->get_metrics();
    // metrics contains total queries, avg latency, success rate, etc.

    auto health = db->check_health();

    return 0;
}
```

### Example 4: Connection String Builder Usage

```cpp
#include "database/integrated/connection_string_builder.h"

using namespace database::integrated;

void demonstrate_builders() {
    // PostgreSQL with full options
    auto pg = connection_string_builder()
        .host("pg.example.com")
        .port(5432)
        .database("production")
        .user("admin")
        .password("secret")
        .ssl_mode(ssl_mode::verify_full)
        .connect_timeout(10)
        .application_name("my-service")
        .build(backend_type::postgres);
    // → "host=pg.example.com port=5432 dbname=production user=admin
    //    password=secret sslmode=verify-full connect_timeout=10
    //    application_name=my-service"


    // SQLite file-based
    auto sqlite_file = connection_string_builder()
        .database("/var/data/app.db")
        .build(backend_type::sqlite);
    // → "/var/data/app.db"

    // SQLite in-memory
    auto sqlite_mem = connection_string_builder()
        .in_memory()
        .build(backend_type::sqlite);
    // → ":memory:"

    // Reusable builder
    auto builder = connection_string_builder()
        .host("localhost")
        .user("admin")
        .password("secret");

    auto dev = builder.database("dev_db").build(backend_type::postgres);
    builder.reset();
    auto test = connection_string_builder()
        .host("localhost")
        .user("admin")
        .password("secret")
        .database("test_db")
        .build(backend_type::postgres);
}
```

---

## Configuration Defaults Reference

| Subsystem | Parameter | Default |
|-----------|-----------|---------|
| **Database** | Backend type | `postgres` |
| | Connection string | `host=localhost port=5432 dbname=postgres` |
| | SSL | Disabled |
| | Prepared statements | Enabled |
| | Query cache | Disabled |
| **Pool** | Name | `"default_pool"` |
| | Min connections | 2 |
| | Max connections | 10 |
| | Acquisition timeout | 30s |
| | Idle timeout | 300s (5 min) |
| | Health checks | Enabled (60s interval) |
| **Logger** | Query logging | Disabled |
| | Connection logging | Enabled |
| | Slow query detection | Enabled (1000ms threshold) |
| | Log level | `info` |
| | File logging | Disabled |
| | Rotation | 10 MB, 5 files |
| **Monitoring** | Metrics collection | Enabled |
| | Profiling | Disabled |
| | Health checks | Enabled |
| | Metrics interval | 60s |
| | Pool usage warning | 80% |
| | Latency warning | 500ms |
| | Prometheus | Disabled |
| **Thread Pool** | Name | `"db_thread_pool"` |
| | Worker threads | 0 (auto-detect) |
| | Max queue size | 1000 |
| | Priority scheduling | Disabled |
| | Pool type | `standard` |
| **Integration** | common_system | Enabled |
| | thread_system | Enabled |
| | monitoring_system | Enabled |

---

## Limitations and Known Issues

| Area | Limitation | Details |
|------|-----------|---------|
| Backend support | Only PostgreSQL implemented | `create_backend()` in `unified_database_system.cpp` currently only supports `backend_type::postgres`. Other backend types require additional implementation. |
| Thread safety | `unified_database_system::impl` uses `std::mutex` | Single mutex for all state — may contend under high concurrency. Consider lock-free structures for hot paths. |
| Connection pool | Phase 6 integration pending | `database_coordinator` reserves Phase 6 for connection pool adapter integration. |
| Latency tracking | EMA-based | `unified_database_system::impl` uses exponential moving average for latency tracking, which may not capture sudden spikes accurately. |
| `connection_string_builder` | Not thread-safe | Each thread should use its own builder instance. |
| Transaction | Auto-rollback only | `transaction_impl` destructor auto-rolls back uncommitted transactions. No savepoint support. |
| Monitoring | No system backend for monitoring | Unlike `logger_adapter` which has a `system` backend, `monitoring_adapter`'s `system` backend requires `HAVE_SYSTEM_MONITORING_BACKEND` which is not yet available. |

---

## Related Documentation

- [Architecture Overview](../ARCHITECTURE.md) — System-wide architecture and design patterns
- [API Reference](../API_REFERENCE.md) — Complete API reference for all components
- [ORM Guide](../ORM_GUIDE.md) — Object-Relational Mapping framework
- [FEATURES](../FEATURES.md) — Feature overview and project roadmap

---

*Last Updated: 2025-10-20*
