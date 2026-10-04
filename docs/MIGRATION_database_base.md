---
doc_id: "DBS-MIGR-001"
doc_title: "Migrating from database_base to database_backend"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "MIGR"
---

# Migrating from database_base to database_backend

> **SSOT**: This document is the single source of truth for **Migrating from database_base to database_backend**.
>
> **Related**: [migration/database_base.md](migration/database_base.md) previously duplicated this topic from a `GUID` perspective. As of #592 this document is the chosen canonical SSOT, and the `GUID` copy has been collapsed into a redirect stub pointing here.


## Overview

As of Issue #287, `database_base` is deprecated in favor of the new `database_backend` interface. This guide helps you migrate your code.

## Timeline

- **v0.4.x**: `database_base` deprecated, `database_backend` recommended
- **v0.4.4**: Legacy manager implementations removed (sqlite_manager, legacy mysql_manager, mongodb_manager, redis_manager)
- **v0.5.0.0**: `database_base` interface will be removed completely

## Removed Legacy Managers

As of v0.4.4 (#304), the following legacy manager implementations have been removed:

| Removed File | Replacement |
|--------------|-------------|
| `database/backends/sqlite/sqlite_manager.{h,cpp}` | `sqlite_backend` |
| `database/backends/mysql/mysql_manager.{h,cpp}` | `legacy mysql_backend` |
| `database/backends/mongodb/mongodb_manager.{h,cpp}` | `mongodb_backend` |
| `database/backends/redis/redis_manager.{h,cpp}` | `redis_backend` |

These legacy managers implemented `database_base` and used the old API with bool/unsigned int returns.
All new development should use the `*_backend` implementations with `Result<T>` error handling.

## Key Changes

### 1. Base Class Changed

```cpp
// Before (deprecated)
#include "database/database_base.h"
class my_backend : public database_base { ... };

// After
#include "database/core/database_backend.h"
class my_backend : public core::database_backend { ... };
```

### 2. Method Signatures Changed

| database_base (deprecated) | database_backend (new) |
|---------------------------|------------------------|
| `database_types database_type()` | `database_types type() const` |
| `bool connect(const std::string&)` | `VoidResult initialize(const connection_config&)` |
| `bool disconnect()` | `VoidResult shutdown()` |
| `unsigned int insert_query(const std::string&)` | `Result<uint64_t> insert_query(const std::string&)` |
| `unsigned int update_query(const std::string&)` | `Result<uint64_t> update_query(const std::string&)` |
| `unsigned int delete_query(const std::string&)` | `Result<uint64_t> delete_query(const std::string&)` |
| `database_result select_query(const std::string&)` | `Result<database_result> select_query(const std::string&)` |
| `bool execute_query(const std::string&)` | `VoidResult execute_query(const std::string&)` |
| `bool create_query(const std::string&)` | Use `execute_query()` |

### 3. Return Types Use Result<T>

```cpp
// Before: bool/unsigned int return, check error manually
bool connected = db->connect("...");
if (!connected) {
    // How to get error message?
}

// After: Result<T> with error information
auto result = backend->initialize(config);
if (!result.is_ok()) {
    std::cerr << result.error().message << std::endl;
}
```

### 4. New Features Available

```cpp
// Transaction support
backend->begin_transaction();
backend->commit_transaction();
backend->rollback_transaction();
backend->in_transaction();

// Connection info for monitoring
auto info = backend->connection_info();

// Error handling
std::string error = backend->last_error();
```

## Migration Steps

### Step 1: Update Includes

```cpp
// Before
#include "database/database_base.h"

// After
#include "database/core/database_backend.h"
```

### Step 2: Update Class Inheritance

```cpp
// Before
class CustomBackend : public database::database_base

// After
class CustomBackend : public database::core::database_backend
```

### Step 3: Update Method Implementations

```cpp
// Before
bool CustomBackend::connect(const std::string& conn_str) {
    // connection logic
    return success;
}

// After
kcenon::common::VoidResult CustomBackend::initialize(
    const core::connection_config& config) {
    // connection logic
    if (success) {
        return kcenon::common::ok();
    }
    return kcenon::common::error_info{-1, "Connection failed", "CustomBackend"};
}
```

### Step 4: Update Query Methods

```cpp
// Before
unsigned int CustomBackend::insert_query(const std::string& query) {
    // execute query
    return rows_affected;
}

// After
kcenon::common::Result<uint64_t> CustomBackend::insert_query(
    const std::string& query) {
    // execute query
    if (success) {
        return kcenon::common::Result<uint64_t>(rows_affected);
    }
    return kcenon::common::error_info{-1, last_error_, "CustomBackend"};
}
```

### Step 5: Add New Required Methods

```cpp
// New methods required by database_backend
bool CustomBackend::is_initialized() const { return initialized_; }

VoidResult CustomBackend::begin_transaction() { /* ... */ }
VoidResult CustomBackend::commit_transaction() { /* ... */ }
VoidResult CustomBackend::rollback_transaction() { /* ... */ }
bool CustomBackend::in_transaction() const { return in_transaction_; }

std::string CustomBackend::last_error() const { return last_error_; }
std::map<std::string, std::string> CustomBackend::connection_info() const {
    return {{"backend", "custom"}, {"status", "connected"}};
}
```

## Using database_manager (Unchanged API)

The `database_manager` public API remains backward compatible:

```cpp
// This still works
auto db = std::make_shared<database_manager>(context);
db->set_mode(database_types::postgres);
db->connect("host=localhost ...");

unsigned int rows = db->insert_query("INSERT INTO ...");
auto results = db->select_query("SELECT * FROM ...");
db->disconnect();
```

### New Result-based API

```cpp
// New API with better error handling
auto result = db->insert_query_result("INSERT INTO ...");
if (result.is_ok()) {
    std::cout << "Inserted " << result.value() << " rows" << std::endl;
} else {
    std::cerr << result.error().message << std::endl;
}

// Transaction support
db->begin_transaction();
db->insert_query("...");
db->commit_transaction();
```

## Using database_base_adapter

For gradual migration, use `database_base_adapter` to wrap legacy code:

```cpp
#include "database/database_base_adapter.h"

// Wrap legacy database_base in adapter
auto legacy = std::make_unique<MyLegacyDatabase>();
auto adapted = std::make_unique<database_base_adapter>(std::move(legacy));

// Now use as database_backend
database_manager dm(context);
// Use adapted with new interface
```

## FAQ

### Q: Can I still use database_base?

Yes, but you'll see deprecation warnings. Plan to migrate before v0.5.0.0.

### Q: Does database_manager still work with my code?

Yes, `database_manager`'s public API is backward compatible. Internal changes don't affect your usage.

### Q: What about proxy_connector?

`proxy_connector` now implements `database_backend` instead of `database_base`. This is transparent if you use it via `database_manager`.

## Test Mocks

For testing, use the new `mock_backend` instead of `mock_database`:

```cpp
// Before (deprecated)
#include "mocks/mock_database.h"
mock_database db;
db.expect_query("SELECT * FROM users").will_return(test_data);

// After
#include "mocks/mock_backend.h"
mock_backend db;
db.expect_query("SELECT * FROM users").will_return(test_data);

// Modern mock with Result<T> types
auto result = db.select_query("SELECT * FROM users");
EXPECT_TRUE(result.is_ok());
EXPECT_EQ(result.value().size(), 3);
```

The `mock_backend` class provides:
- `expect_query()` - Exact match expectations
- `expect_pattern()` - Regex pattern matching
- `expect_any()` - Match any query
- Full `Result<T>` support for proper error testing
- Transaction state tracking
- Query history for verification

## Suppressing Deprecation Warnings

For legacy code that must use `database_base`:

```cpp
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#include "database/database_base.h"
// Your legacy code here

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
```

## See Also

- [database_backend.h](../database/core/database_backend.h) - New interface
- [database_base.h](../database/database_base.h) - Deprecated interface (for reference)
- [database_base_adapter.h](../database/database_base_adapter.h) - Adapter for gradual migration
- [mock_backend.h](../tests/mocks/mock_backend.h) - Test mock for database_backend

> **Note**: The `adapter_usage_example.cpp` sample has been removed as legacy managers are no longer available.
> Use the new `*_backend` implementations directly instead.
