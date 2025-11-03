# Architecture Design Document

## 🏗️ Overview

This document describes the architecture for integrating adapter pattern into database_system, following the proven design of `integrated_thread_system`.

## 🎨 Design Principles

### 1. Adapter Pattern
**Each external system has a dedicated adapter** that:
- Provides a unified interface
- Handles conditional compilation
- Implements fallback when system unavailable
- Uses PIMPL idiom for ABI stability

### 2. Coordinator Pattern
**System coordinator manages lifecycle**:
- Controls initialization order
- Ensures graceful shutdown
- Provides centralized access to adapters
- Handles error propagation

### 3. Configuration-Driven
**Unified configuration structure**:
- Builder pattern for easy setup
- Smart defaults (zero-config)
- Runtime and compile-time configurability
- Type-safe enums and structs

### 4. Zero-Dependency Fallback
**Works without external systems**:
- Conditional compilation (`#if defined(USE_*_SYSTEM)`)
- Fallback implementations using stdlib
- No runtime errors if systems missing
- Graceful degradation

## 📐 Layer Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                    │
│           (User Code, Business Logic)                   │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│              Unified Database System                    │
│  ┌──────────────────────────────────────────────┐      │
│  │  unified_database_system (Public API)        │      │
│  │  - execute_query()                           │      │
│  │  - begin_transaction()                       │      │
│  │  - get_metrics()                             │      │
│  └────────────────┬─────────────────────────────┘      │
└───────────────────┼─────────────────────────────────────┘
                    │
┌───────────────────▼─────────────────────────────────────┐
│              Database Coordinator                       │
│  ┌──────────────────────────────────────────────┐      │
│  │  database_coordinator                        │      │
│  │  - Lifecycle management                      │      │
│  │  - Adapter orchestration                     │      │
│  │  - Error handling                            │      │
│  └────────┬─────────┬──────────┬─────────┬──────┘      │
└───────────┼─────────┼──────────┼─────────┼─────────────┘
            │         │          │         │
    ┌───────▼───┐ ┌──▼────┐ ┌───▼────┐ ┌─▼──────┐
    │ logger_   │ │monitor│ │thread_ │ │common_ │
    │ adapter   │ │adapter│ │adapter │ │adapter │
    └─────┬─────┘ └───┬───┘ └────┬───┘ └────┬───┘
          │           │          │          │
          │           │          │          │
    ┌─────▼─────┐ ┌───▼───┐ ┌────▼───┐ ┌────▼───┐
    │logger_    │ │monitor│ │thread_ │ │common_ │
    │system     │ │system │ │system  │ │system  │
    └───────────┘ └───────┘ └────────┘ └────────┘
          OR          OR        OR         OR
    ┌─────▼─────┐ ┌───▼───┐ ┌────▼───┐ ┌────▼───┐
    │std::cout  │ │internal│ │std::   │ │Optional│
    │std::ofstre│ │metrics │ │thread  │ │Result  │
    └───────────┘ └───────┘ └────────┘ └────────┘
        FALLBACK   FALLBACK   FALLBACK   FALLBACK
```

## 🔧 Component Details

### 1. Configuration Layer (`database/integrated/core/configuration.h`)

**Purpose**: Centralized configuration for all subsystems

**Components**:

```cpp
namespace database::integrated {

// Enums
enum class db_log_level { trace, debug, info, warning, error, critical };
enum class backend_type { postgres, mysql, sqlite, mongodb, redis };

// Configuration structs
struct pool_config { /* connection pool settings */ };
struct db_thread_config { /* async thread pool settings */ };
struct db_logger_config { /* logging settings */ };
struct db_monitoring_config { /* monitoring settings */ };
struct database_config { /* database-specific settings */ };

// Unified configuration with builder pattern
struct unified_db_config {
    database_config database;
    pool_config connection_pool;
    db_thread_config thread;
    db_logger_config logger;
    db_monitoring_config monitoring;

    // Builder methods
    unified_db_config& set_backend(...);
    unified_db_config& set_pool_size(...);
    unified_db_config& set_log_level(...);
    unified_db_config& enable_monitoring(...);
};

} // namespace database::integrated
```

**Design Decisions**:
- **Builder pattern**: Fluent API for easy configuration
- **Smart defaults**: Zero-config works out of the box
- **Type-safe**: Enums instead of strings/integers
- **Extensible**: Easy to add new options

---

### 2. Logger Adapter (`database/integrated/adapters/logger_adapter.h/cpp`)

**Purpose**: Unified logging interface with fallback

**Interface**:

```cpp
class logger_adapter {
public:
    explicit logger_adapter(const db_logger_config& config);
    ~logger_adapter();

    common::VoidResult initialize();
    common::VoidResult shutdown();

    // Database-specific logging
    void log_query(db_log_level, const std::string& query,
                   std::chrono::microseconds duration);
    void log_slow_query(const std::string& query, ...);
    void log_connection_event(const std::string& event, ...);
    void log_transaction(const std::string& operation, bool success, ...);
    void log_pool_event(...);
    void log_error(...);

    // Generic logging
    void log(db_log_level level, const std::string& message);
    void flush();

private:
    class impl;  // PIMPL idiom
    std::unique_ptr<impl> pimpl_;
};
```

**Implementation Strategy**:

```cpp
// logger_adapter.cpp
#if defined(USE_LOGGER_SYSTEM)
    // Use logger_system::logger
    #include <kcenon/logger/core/logger.h>
    #include <kcenon/logger/writers/console_writer.h>
    #include <kcenon/logger/writers/file_writer.h>

    class logger_adapter::impl {
        std::unique_ptr<kcenon::logger::logger> logger_;
        // ... implementation using logger_system
    };
#else
    // Fallback: std::cout + std::ofstream
    #include <iostream>
    #include <fstream>
    #include <mutex>

    class logger_adapter::impl {
        std::mutex mutex_;
        std::ofstream file_;
        // ... fallback implementation
    };
#endif
```

**Features**:
- **SQL sanitization**: Removes passwords, truncates long queries
- **Slow query detection**: Automatic warning for queries exceeding threshold
- **Structured logging**: Consistent format with timestamps
- **Thread-safe**: Mutex-protected or async logger
- **Multiple outputs**: Console + file

---

### 3. Monitoring Adapter (`database/integrated/adapters/monitoring_adapter.h/cpp`)

**Purpose**: Performance metrics and health checks

**Interface**:

```cpp
struct database_metrics {
    // Connection pool
    std::size_t active_connections;
    std::size_t idle_connections;
    double connection_usage_percent;

    // Query performance
    std::uint64_t total_queries;
    std::uint64_t successful_queries;
    double query_success_rate;
    std::chrono::microseconds avg_query_latency;
    std::chrono::microseconds p95_query_latency;

    // Transactions
    std::uint64_t active_transactions;
    std::uint64_t committed_transactions;
    std::uint64_t rolled_back_transactions;
};

class monitoring_adapter : public common::interfaces::IMonitor {
public:
    explicit monitoring_adapter(const db_monitoring_config& config);
    ~monitoring_adapter() override;

    common::VoidResult initialize();
    common::VoidResult shutdown();

    // IMonitor interface
    common::VoidResult record_metric(const std::string& name, double value) override;
    common::Result<metrics_snapshot> get_metrics() override;
    common::Result<health_check_result> check_health() override;

    // Database-specific
    void record_query_execution(std::chrono::microseconds duration, bool success);
    void record_connection_acquired();
    void record_connection_released();
    void update_pool_stats(std::size_t active, std::size_t idle, std::size_t total);

    common::Result<database_metrics> get_database_metrics();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};
```

**Implementation Strategy**:

```cpp
#if defined(USE_MONITORING_SYSTEM)
    #include <kcenon/monitoring/core/performance_profiler.h>
    #include <kcenon/monitoring/core/system_monitor.h>

    class monitoring_adapter::impl {
        std::unique_ptr<kcenon::monitoring::performance_profiler> profiler_;
        std::unique_ptr<kcenon::monitoring::system_monitor> monitor_;
        // ... uses monitoring_system
    };
#else
    // Fallback: Bridge to existing internal metrics
    #include "../monitoring/pool_metrics.h"
    #include "../monitoring/performance_monitor.h"

    class monitoring_adapter::impl {
        pool_metrics pool_metrics_;
        performance_monitor perf_monitor_;
        // ... uses internal implementation
    };
#endif
```

**Features**:
- **Prometheus export**: Standard metrics format
- **Health checks**: Connection pool, database connectivity
- **Statistical profiling**: Mean, P95, P99 latencies
- **Thresholds**: Configurable warning/critical levels
- **Bridge pattern**: Integrates existing internal metrics

---

### 4. Thread Adapter (`database/integrated/adapters/thread_adapter.h/cpp`)

**Purpose**: Unified async execution interface

**Interface**:

```cpp
class thread_adapter {
public:
    explicit thread_adapter(const db_thread_config& config);
    ~thread_adapter();

    common::VoidResult initialize();
    common::VoidResult shutdown();

    // Task execution
    common::VoidResult execute(std::function<void()> task);

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    template<typename F, typename... Args>
    auto submit_with_priority(int priority, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // Cancellation support
    std::shared_ptr<void> create_cancellation_token();
    void cancel_token(std::shared_ptr<void> token);

    template<typename F, typename... Args>
    auto submit_cancellable(std::shared_ptr<void> token, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // Stats
    std::size_t worker_count() const;
    std::size_t queue_size() const;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};
```

**Replaces**: `database/adapters/thread_pool_adapter.h`

**Features**:
- **Priority scheduling**: High-priority queries first
- **Cancellation tokens**: Stop long-running queries
- **Work stealing**: Load balancing across threads
- **Fallback**: std::thread + std::packaged_task

---

### 5. Database Coordinator (`database/integrated/core/database_coordinator.h/cpp`)

**Purpose**: Lifecycle management and adapter orchestration

**Interface**:

```cpp
class database_coordinator {
public:
    explicit database_coordinator(const unified_db_config& config);
    ~database_coordinator();

    // Lifecycle
    common::VoidResult initialize();
    common::VoidResult shutdown();
    bool is_initialized() const;

    // Adapter access
    adapters::logger_adapter* get_logger();
    adapters::monitoring_adapter* get_monitor();
    adapters::thread_adapter* get_thread_pool();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};
```

**Initialization Order** (Critical!):

```cpp
common::VoidResult database_coordinator::impl::initialize() {
    // 1. Logger first (for debugging other initializations)
    auto logger_result = logger_->initialize();
    if (!logger_result.is_ok()) {
        return logger_result;
    }
    logger_->log(db_log_level::info, "Logger initialized");

    // 2. Monitoring second (to track initialization metrics)
    auto monitor_result = monitor_->initialize();
    if (!monitor_result.is_ok()) {
        logger_->log(db_log_level::error, "Monitor init failed");
        return monitor_result;
    }
    logger_->log(db_log_level::info, "Monitoring initialized");

    // 3. Thread pool third (for async operations)
    auto thread_result = thread_pool_->initialize();
    if (!thread_result.is_ok()) {
        logger_->log(db_log_level::error, "Thread pool init failed");
        return thread_result;
    }
    logger_->log(db_log_level::info, "Thread pool initialized");

    // 4. Database connection pool last
    // (uses logger, monitor, thread_pool)
    // ... connection pool initialization

    return common::ok();
}
```

**Shutdown Order** (Reverse of initialization):

```cpp
common::VoidResult database_coordinator::impl::shutdown() {
    // 1. Connection pool first
    // 2. Thread pool second
    // 3. Monitoring third
    // 4. Logger last (keep logging until the end)
    return common::ok();
}
```

---

### 6. Unified Database System (`database/integrated/unified_database_system.h/cpp`)

**Purpose**: User-facing API with zero-config setup

**Interface**:

```cpp
class unified_database_system {
public:
    unified_database_system();
    explicit unified_database_system(const unified_db_config& config);
    ~unified_database_system();

    // Initialization
    common::VoidResult initialize();
    common::VoidResult initialize(const unified_db_config& config);
    common::VoidResult shutdown();

    // Query execution
    common::Result<database_result> execute_query(const std::string& query);

    std::future<common::Result<database_result>>
    execute_query_async(const std::string& query);

    std::future<common::Result<database_result>>
    execute_query_priority(const std::string& query, int priority);

    // Transactions
    common::VoidResult begin_transaction();
    common::VoidResult commit();
    common::VoidResult rollback();

    // Observability
    common::Result<database_metrics> get_metrics();
    common::Result<health_check_result> check_health();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};
```

**Usage Example**:

```cpp
// Zero-config
unified_database_system db;
db.initialize(unified_db_config{}
    .set_backend(backend_type::postgres, "host=localhost dbname=test")
);

auto result = db.execute_query("SELECT * FROM users");
if (result.is_ok()) {
    for (const auto& row : result.value()) {
        // ...
    }
}

auto metrics = db.get_metrics();
std::cout << "Query success rate: " << metrics.value().query_success_rate << "\n";
```

---

## 🔄 Integration Points

### Connection Pool V2 Integration

**Before**:
```cpp
// connection_pool_v2.cpp - Direct usage
#ifdef USE_THREAD_SYSTEM
    #include <kcenon/thread/core/typed_thread_pool.h>
    typed_thread_pool_t<connection_priority> pool_;
#endif
```

**After**:
```cpp
// connection_pool_v2.cpp - Via adapter
#include "integrated/adapters/thread_adapter.h"
#include "integrated/adapters/logger_adapter.h"
#include "integrated/adapters/monitoring_adapter.h"

class connection_pool_v2::impl {
    thread_adapter* thread_pool_;
    logger_adapter* logger_;
    monitoring_adapter* monitor_;

    void acquire_connection(connection_priority priority) {
        logger_->log(db_log_level::debug, "Acquiring connection");
        monitor_->record_connection_acquired();

        auto future = thread_pool_->submit_with_priority(
            static_cast<int>(priority),
            [this]() { /* ... */ }
        );
        // ...
    }
};
```

### Async Executor V2 Integration

**Before**:
```cpp
// async_executor_v2.h - Direct usage
#ifdef USE_THREAD_SYSTEM
    kcenon::thread::thread_pool pool_;
#endif
```

**After**:
```cpp
// async_executor_v2.h - Via adapter
#include "integrated/adapters/thread_adapter.h"

class async_executor_v2 {
    thread_adapter* thread_pool_;

    template<typename F>
    auto execute_async(F&& func) {
        return thread_pool_->submit(std::forward<F>(func));
    }
};
```

---

## 📂 Directory Structure

```
database_system/
├── database/
│   ├── integrated/                              [NEW]
│   │   ├── core/
│   │   │   ├── configuration.h                  [Config structs, enums]
│   │   │   ├── configuration.cpp                [Default values]
│   │   │   ├── database_coordinator.h           [Lifecycle manager]
│   │   │   └── database_coordinator.cpp         [Implementation]
│   │   ├── adapters/
│   │   │   ├── logger_adapter.h                 [Logger interface]
│   │   │   ├── logger_adapter.cpp               [Logger impl + fallback]
│   │   │   ├── monitoring_adapter.h             [Monitoring interface]
│   │   │   ├── monitoring_adapter.cpp           [Monitor impl + fallback]
│   │   │   ├── thread_adapter.h                 [Thread interface]
│   │   │   └── thread_adapter.cpp               [Thread impl + fallback]
│   │   ├── unified_database_system.h            [Public API]
│   │   ├── unified_database_system.cpp          [Implementation]
│   │   └── CMakeLists.txt                       [Build configuration]
│   ├── adapters/
│   │   ├── common_system_adapter.h              [KEEP - enhanced]
│   │   └── thread_pool_adapter.h                [DEPRECATE - migrate to integrated/]
│   ├── pooling/
│   │   ├── connection_pool.h                    [KEEP - legacy API]
│   │   └── connection_pool_v2.h                 [UPDATE - use adapters]
│   ├── monitoring/
│   │   ├── pool_metrics.h                       [KEEP - bridged]
│   │   └── performance_monitor.h                [KEEP - bridged]
│   └── CMakeLists.txt                           [UPDATE - add integrated/]
└── .integration_plan/                           [TEMPORARY - delete when done]
```

---

## 🔒 Design Patterns Used

### 1. Adapter Pattern
- **Purpose**: Provide unified interface to different systems
- **Implementation**: Each adapter wraps an external system or fallback
- **Benefits**: Decoupling, testability, flexibility

### 2. PIMPL (Pointer to Implementation)
- **Purpose**: Hide implementation details, improve ABI stability
- **Implementation**: `class impl;` forward declaration + `unique_ptr<impl>`
- **Benefits**: Fast compilation, ABI stability, encapsulation

### 3. Builder Pattern
- **Purpose**: Fluent configuration API
- **Implementation**: `unified_db_config& set_*(...)` returning `*this`
- **Benefits**: Readable, flexible, discoverable

### 4. Coordinator/Facade Pattern
- **Purpose**: Simplify subsystem interactions
- **Implementation**: `database_coordinator` manages all adapters
- **Benefits**: Single point of control, clear responsibility

### 5. Strategy Pattern
- **Purpose**: Swap implementations at compile-time
- **Implementation**: Conditional compilation for system vs fallback
- **Benefits**: Zero runtime overhead, maximum flexibility

---

## 🎯 Design Goals Achieved

✅ **Modularity**: Each adapter is independent
✅ **Testability**: Adapters can be mocked
✅ **Flexibility**: Compile-time and runtime configuration
✅ **Performance**: Zero overhead abstractions
✅ **Maintainability**: Clear separation of concerns
✅ **Compatibility**: Legacy API preserved
✅ **Observability**: Built-in logging and monitoring
✅ **Reliability**: Graceful degradation with fallbacks

---

**Document Version**: 1.0
**Last Updated**: 2025-11-03
