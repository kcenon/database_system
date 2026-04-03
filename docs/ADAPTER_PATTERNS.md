---
doc_id: "DBS-GUID-001"
doc_title: "Adapter Pattern Best Practices"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Adapter Pattern Best Practices

> **SSOT**: This document is the single source of truth for **Adapter Pattern Best Practices**.

> **Language:** **English** | [한국어](ADAPTER_PATTERNS.kr.md)

**Version:** 1.0.0
**Last Updated:** 2025-12-27
**Status:** Reference Documentation

This document describes the adapter pattern implementation used in database_system for managing optional dependencies. This pattern achieves **ZERO circular dependency risk** while maintaining flexibility and testability.

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [When to Use Adapter Pattern](#when-to-use-adapter-pattern)
- [Implementation Guide](#implementation-guide)
  - [Step 1: Define Abstract Backend Interface](#step-1-define-abstract-backend-interface)
  - [Step 2: Implement Fallback Backend](#step-2-implement-fallback-backend)
  - [Step 3: Implement Null Backend](#step-3-implement-null-backend)
  - [Step 4: Implement System Backend (Optional)](#step-4-implement-system-backend-optional)
  - [Step 5: Create Adapter with Factory](#step-5-create-adapter-with-factory)
- [Implementation Checklist](#implementation-checklist)
- [Code Examples](#code-examples)
  - [Monitoring Adapter](#monitoring-adapter)
  - [Thread Adapter](#thread-adapter)
  - [Logger Adapter](#logger-adapter)
- [Anti-Patterns to Avoid](#anti-patterns-to-avoid)
- [Decision Tree](#decision-tree)
- [Benefits](#benefits)
- [Related Documentation](#related-documentation)

---

## Overview

The adapter pattern in database_system provides a unified interface for optional dependencies with runtime backend selection. This approach eliminates conditional compilation complexity while maintaining:

- **Zero circular dependency risk** - All optional dependencies use abstract interfaces
- **Runtime flexibility** - Backend selection at runtime, not compile-time
- **Full testability** - Null backends enable testing without external dependencies
- **Graceful degradation** - Fallback backends work when optional systems are unavailable

### Dependency Structure

```
database_system (Tier 3)
       │
       ├── common_system (REQUIRED)
       ├── thread_system (OPTIONAL - adapter pattern)
       ├── container_system (OPTIONAL - protocol container)
       ├── monitoring_system (OPTIONAL - metrics adapter)
       └── External: PostgreSQL, SQLite, MongoDB, Redis
```

---

## Architecture

### Pattern Components

```
┌─────────────────────────────────────────────────────────────┐
│                      Adapter Class                          │
│  (monitoring_adapter, thread_adapter, logger_adapter)       │
├─────────────────────────────────────────────────────────────┤
│                    Factory Method                           │
│              create_backend(config, type)                   │
├─────────────────────────────────────────────────────────────┤
│               Abstract Backend Interface                    │
│  (monitoring_backend, thread_backend, logger_backend)       │
├──────────────┬──────────────┬──────────────┬───────────────┤
│    System    │   Fallback   │     Null     │   (Future)    │
│   Backend    │   Backend    │   Backend    │   Backends    │
│ (optional)   │  (default)   │  (testing)   │               │
└──────────────┴──────────────┴──────────────┴───────────────┘
```

### Backend Types

| Type | Purpose | Dependencies |
|------|---------|--------------|
| `system` | Uses external system (e.g., monitoring_system) | Requires external dependency |
| `fallback` | Self-contained implementation | No external dependencies |
| `null` | No-op implementation | No external dependencies |
| `auto_select` | Automatically selects best available | Runtime detection |

---

## When to Use Adapter Pattern

### Use Adapter Pattern When:

1. **Optional external dependency** - The feature should work without the dependency
2. **Multiple implementations possible** - Different backends for different environments
3. **Testing isolation needed** - Need to test without external systems
4. **Runtime selection required** - Backend selection cannot be compile-time

### Keep Direct Dependency When:

1. **Always required** - The dependency is mandatory (e.g., common_system)
2. **Single implementation** - Only one way to implement the feature
3. **No testing isolation needed** - External system is always available in tests

---

## Implementation Guide

### Step 1: Define Abstract Backend Interface

Create a pure virtual interface that all backends must implement:

```cpp
// backends/monitoring_backend.h

class monitoring_backend {
public:
    virtual ~monitoring_backend() = default;

    // Lifecycle
    virtual common::VoidResult initialize() = 0;
    virtual common::VoidResult shutdown() = 0;
    virtual bool is_initialized() const = 0;

    // Core operations
    virtual common::VoidResult record_metric(
        const std::string& name, double value) = 0;
    virtual common::Result<metrics_snapshot> get_metrics() = 0;
    virtual common::VoidResult reset() = 0;

    // Domain-specific operations
    virtual void record_query_execution(
        std::chrono::microseconds duration, bool success) = 0;
};
```

**Key principles:**
- Pure virtual destructor with default implementation
- All methods are pure virtual
- No dependency on external types in interface
- Use `common::Result<T>` for error handling

### Step 2: Implement Fallback Backend

Create a self-contained implementation that works without external dependencies:

```cpp
// backends/fallback_monitoring_backend.h

class fallback_monitoring_backend : public monitoring_backend {
public:
    explicit fallback_monitoring_backend(const db_monitoring_config& config);
    ~fallback_monitoring_backend() override;

    common::VoidResult initialize() override;
    common::VoidResult shutdown() override;
    bool is_initialized() const override;

    common::VoidResult record_metric(
        const std::string& name, double value) override;
    common::Result<metrics_snapshot> get_metrics() override;
    common::VoidResult reset() override;

    void record_query_execution(
        std::chrono::microseconds duration, bool success) override;

private:
    const db_monitoring_config& config_;
    bool initialized_;
    mutable std::mutex mutex_;

    // Internal storage
    std::unordered_map<std::string, double> metrics_;
    std::vector<std::chrono::microseconds> query_latencies_;
};
```

**Key principles:**
- Thread-safe with internal synchronization
- Provides meaningful (not stub) functionality
- Can be used for testing
- No external dependencies

### Step 3: Implement Null Backend

Create a no-op implementation for testing and disabled scenarios:

```cpp
// backends/null_monitoring_backend.h

class null_monitoring_backend : public monitoring_backend {
public:
    explicit null_monitoring_backend(const db_monitoring_config& /*config*/) {}
    ~null_monitoring_backend() override = default;

    common::VoidResult initialize() override {
        return common::ok();
    }

    common::VoidResult shutdown() override {
        return common::ok();
    }

    bool is_initialized() const override {
        return true;
    }

    common::VoidResult record_metric(
        const std::string& /*name*/, double /*value*/) override {
        return common::ok();
    }

    common::Result<metrics_snapshot> get_metrics() override {
        return metrics_snapshot{};
    }

    common::VoidResult reset() override {
        return common::ok();
    }

    void record_query_execution(
        std::chrono::microseconds /*duration*/, bool /*success*/) override {
        // No-op
    }
};
```

**Key principles:**
- All methods are no-ops or return empty/default values
- Header-only implementation (inline methods)
- Zero overhead when used
- Always succeeds (never returns errors)

### Step 4: Implement System Backend (Optional)

Create an implementation that uses the external system when available:

```cpp
// backends/system_monitoring_backend.h

#ifdef HAVE_MONITORING_SYSTEM
class system_monitoring_backend : public monitoring_backend {
public:
    explicit system_monitoring_backend(const db_monitoring_config& config);
    ~system_monitoring_backend() override;

    common::VoidResult initialize() override;
    common::VoidResult shutdown() override;
    bool is_initialized() const override;

    common::VoidResult record_metric(
        const std::string& name, double value) override;
    common::Result<metrics_snapshot> get_metrics() override;
    common::VoidResult reset() override;

    void record_query_execution(
        std::chrono::microseconds duration, bool success) override;

private:
    std::unique_ptr<monitoring_system::monitor> monitor_;
};
#endif
```

**Key principles:**
- Guarded with `#ifdef` preprocessor directive
- Uses external dependency internally only
- Same interface as other backends
- Optional - only compiled when dependency is available

### Step 5: Create Adapter with Factory

Create the adapter class with factory method for backend selection:

```cpp
// monitoring_adapter.h

enum class monitoring_backend_type {
    auto_select,  // Automatically select best available
    system,       // Use monitoring_system
    fallback,     // Use internal metrics storage
    null          // No-op backend
};

class monitoring_adapter {
public:
    explicit monitoring_adapter(
        const db_monitoring_config& config,
        monitoring_backend_type backend_type = monitoring_backend_type::auto_select);

    ~monitoring_adapter();

    // Lifecycle
    common::VoidResult initialize();
    common::VoidResult shutdown();
    bool is_initialized() const;

    // Delegated operations
    common::VoidResult record_metric(const std::string& name, double value);
    void record_query_execution(std::chrono::microseconds duration, bool success);

private:
    static std::unique_ptr<backends::monitoring_backend> create_backend(
        const db_monitoring_config& config,
        monitoring_backend_type backend_type);

    const db_monitoring_config& config_;
    std::unique_ptr<backends::monitoring_backend> backend_;
};
```

**Factory implementation:**

```cpp
// monitoring_adapter.cpp

std::unique_ptr<backends::monitoring_backend>
monitoring_adapter::create_backend(
    const db_monitoring_config& config,
    monitoring_backend_type backend_type)
{
    switch (backend_type) {
        case monitoring_backend_type::null:
            return std::make_unique<backends::null_monitoring_backend>(config);

        case monitoring_backend_type::fallback:
            return std::make_unique<backends::fallback_monitoring_backend>(config);

        case monitoring_backend_type::system:
#ifdef HAVE_MONITORING_SYSTEM
            return std::make_unique<backends::system_monitoring_backend>(config);
#else
            // Fall through to fallback if system not available
            [[fallthrough]];
#endif

        case monitoring_backend_type::auto_select:
        default:
#ifdef HAVE_MONITORING_SYSTEM
            return std::make_unique<backends::system_monitoring_backend>(config);
#else
            return std::make_unique<backends::fallback_monitoring_backend>(config);
#endif
    }
}
```

---

## Implementation Checklist

Use this checklist when implementing the adapter pattern:

### Interface Design

- [ ] Abstract base class with virtual destructor
- [ ] Pure virtual methods for all operations
- [ ] No dependency on external types in interface
- [ ] Use `common::Result<T>` for error handling
- [ ] Document thread-safety guarantees

### Fallback Implementation

- [ ] Works without any optional dependencies
- [ ] Provides meaningful (not stub) functionality
- [ ] Thread-safe with internal synchronization
- [ ] Can be used for testing
- [ ] Implements all interface methods

### Null Implementation

- [ ] All methods are no-ops
- [ ] Returns success for all operations
- [ ] Returns empty/default values for queries
- [ ] Header-only for zero overhead
- [ ] Useful for disabled scenarios

### System Implementation (if applicable)

- [ ] Guarded with `#ifdef` preprocessor
- [ ] Uses external dependency internally only
- [ ] Same interface as other backends
- [ ] Graceful failure if dependency unavailable

### Factory Pattern

- [ ] Enum for backend type selection
- [ ] `auto_select` for runtime detection
- [ ] Single point of backend creation
- [ ] Fallback when system unavailable
- [ ] Clear documentation of selection logic

### Adapter Class

- [ ] Holds backend via `std::unique_ptr`
- [ ] Delegates all operations to backend
- [ ] Non-copyable (deleted copy operations)
- [ ] Move-constructible if needed
- [ ] Configuration reference stored

---

## Code Examples

### Monitoring Adapter

**Usage:**

```cpp
#include <database/integrated/adapters/monitoring_adapter.h>

using namespace database::integrated::adapters;

// Configure monitoring
db_monitoring_config config;
config.enable_metrics = true;
config.enable_health_checks = true;
config.metrics_interval = std::chrono::seconds(60);

// Create adapter with auto-selection
monitoring_adapter monitor(config);
auto result = monitor.initialize();
if (!result.is_ok()) {
    std::cerr << "Monitor init failed: " << result.error().message << "\n";
    return;
}

// Record metrics
monitor.record_query_execution(std::chrono::microseconds(1500), true);
monitor.record_connection_acquired();
monitor.update_pool_stats(15, 5, 20);

// Get metrics
auto metrics = monitor.get_database_metrics();
if (metrics.is_ok()) {
    std::cout << "Active connections: " << metrics.value().active_connections << "\n";
}

monitor.shutdown();
```

**Testing with null backend:**

```cpp
// Disable monitoring for unit tests
monitoring_adapter monitor(config, monitoring_backend_type::null);
monitor.initialize();

// All operations succeed but do nothing
monitor.record_query_execution(std::chrono::microseconds(1500), true);
```

### Thread Adapter

**Usage:**

```cpp
#include <database/integrated/adapters/thread_adapter.h>

using namespace database::integrated::adapters;

// Configure thread pool
db_thread_config config;
config.pool_name = "db_async";
config.thread_count = 4;
config.max_queue_size = 1000;

// Create adapter
thread_adapter pool(config);
pool.initialize();

// Submit async task
auto future = pool.submit([]() {
    return execute_query("SELECT * FROM users");
});

// Wait for result
auto result = future.get();

pool.shutdown();
```

### Logger Adapter

**Usage:**

```cpp
#include <database/integrated/adapters/logger_adapter.h>

using namespace database::integrated::adapters;

// Configure logger
db_logger_config config;
config.enable_query_logging = true;
config.log_slow_queries = true;
config.slow_query_threshold = std::chrono::milliseconds(500);

// Create adapter
logger_adapter logger(config);
logger.initialize();

// Log database operations
logger.log_query(db_log_level::info,
    "SELECT * FROM users WHERE id = 123",
    std::chrono::microseconds(1500));

logger.log_connection_event("acquired", "Pool: main_pool");
logger.log_transaction("commit", true, "isolation: read_committed");
logger.log_error("execute_query", "Connection timeout", "08006");

logger.shutdown();
```

---

## Anti-Patterns to Avoid

### 1. Direct `#include` Without Interface Abstraction

**Bad:**
```cpp
#ifdef USE_MONITORING_SYSTEM
    #include <monitoring_system/monitor.h>
    monitoring_system::monitor monitor_;
#else
    internal_monitor monitor_;  // Different type!
#endif
```

**Good:**
```cpp
#include "backends/monitoring_backend.h"
std::unique_ptr<backends::monitoring_backend> backend_;
```

### 2. Compile-Time Only Switching

**Bad:**
```cpp
#ifdef USE_MONITORING_SYSTEM
    void record_metric(const std::string& name, double value) {
        monitor_.record(name, value);
    }
#else
    void record_metric(const std::string& name, double value) {
        // Different implementation
    }
#endif
```

**Good:**
```cpp
void record_metric(const std::string& name, double value) {
    backend_->record_metric(name, value);
}
```

### 3. Fallback That Silently Drops Functionality

**Bad:**
```cpp
class fallback_monitoring_backend : public monitoring_backend {
    void record_metric(const std::string&, double) override {
        // Silently ignored - no metrics stored!
    }
};
```

**Good:**
```cpp
class fallback_monitoring_backend : public monitoring_backend {
    void record_metric(const std::string& name, double value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_[name] = value;  // Actually store the metric
    }
};
```

### 4. Leaky Abstraction

**Bad:**
```cpp
class monitoring_adapter {
public:
    // Exposes backend type in public interface
    monitoring_system::monitor& get_monitor();
};
```

**Good:**
```cpp
class monitoring_adapter {
public:
    // Abstract operations only
    common::VoidResult record_metric(const std::string& name, double value);
    common::Result<metrics_snapshot> get_metrics();
};
```

### 5. Missing Thread Safety in Fallback

**Bad:**
```cpp
class fallback_monitoring_backend {
    void record_metric(const std::string& name, double value) {
        metrics_[name] = value;  // Not thread-safe!
    }
    std::unordered_map<std::string, double> metrics_;
};
```

**Good:**
```cpp
class fallback_monitoring_backend {
    void record_metric(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_[name] = value;
    }
    mutable std::mutex mutex_;
    std::unordered_map<std::string, double> metrics_;
};
```

---

## Decision Tree

Use this decision tree to determine if adapter pattern is appropriate:

```
Is the dependency optional?
├── NO → Use direct dependency
└── YES → Do you need runtime backend selection?
    ├── NO → Use compile-time #ifdef with consistent interface
    └── YES → Do you need testing without the dependency?
        ├── NO → Use simple runtime check
        └── YES → Use full adapter pattern
            ├── Define abstract backend interface
            ├── Implement fallback backend
            ├── Implement null backend (for testing)
            ├── Implement system backend (optional)
            └── Create adapter with factory
```

---

## Benefits

### Zero Circular Dependency Risk

```
database_system → (abstract interface) → monitoring_backend
                                              ↓
                              ┌───────────────┼───────────────┐
                              ↓               ↓               ↓
                         fallback_    system_monitoring  null_monitoring
                         monitoring   (optional #ifdef)   (testing)
```

### Full Testability

```cpp
// Production: auto-select best backend
monitoring_adapter prod_monitor(config);

// Testing: null backend for isolation
monitoring_adapter test_monitor(config, monitoring_backend_type::null);

// Integration: fallback for predictable behavior
monitoring_adapter int_monitor(config, monitoring_backend_type::fallback);
```

### Runtime Flexibility

```cpp
// Select backend based on configuration
monitoring_backend_type type = config.use_external_monitoring
    ? monitoring_backend_type::system
    : monitoring_backend_type::fallback;

monitoring_adapter monitor(config, type);
```

---

## Related Documentation

- [Architecture](ARCHITECTURE.md) - System architecture overview
- [Thread Adapter Evaluation](advanced/THREAD_ADAPTER_EVALUATION.md) - When NOT to use adapter pattern
- [Best Practices](guides/BEST_PRACTICES.md) - General best practices
- [API Reference](API_REFERENCE.md) - Complete API documentation

### Source Code References

| Component | Header | Implementation |
|-----------|--------|----------------|
| Monitoring Adapter | `database/integrated/adapters/monitoring_adapter.h` | `monitoring_adapter.cpp` |
| Thread Adapter | `database/integrated/adapters/thread_adapter.h` | `thread_adapter.cpp` |
| Logger Adapter | `database/integrated/adapters/logger_adapter.h` | `logger_adapter.cpp` |
| Monitoring Backend | `database/integrated/adapters/backends/monitoring_backend.h` | - |
| Fallback Monitoring | `database/integrated/adapters/backends/fallback_monitoring_backend.h` | `.cpp` |
| Null Monitoring | `database/integrated/adapters/backends/null_monitoring_backend.h` | (header-only) |

---

**Last Updated**: 2025-12-27
**Next Review**: 2026-03-27
