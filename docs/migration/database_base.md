---
doc_id: "DBS-GUID-022"
doc_title: "database_base Migration Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# database_base Migration Guide

## Overview

This guide helps you migrate from the legacy `database_base` interface to the modern `database_backend` interface introduced in Sprint 5.

### Why Migrate?

The `database_backend` interface offers significant improvements over `database_base`:

| Feature | database_base | database_backend |
|---------|--------------|------------------|
| Error Handling | `bool` / `unsigned int` returns | `Result<T>` with error details |
| Connection | Raw connection string | Structured `connection_config` |
| Transactions | Not supported | Full transaction support |
| Connection Info | Not available | `connection_info()` method |
| Error Details | No access | `last_error()` method |
| Return Types | Implicit error (0 or false) | Explicit error with message |

## Migration Steps

### Step 1: Understand the Interface Changes

**database_base (Legacy):**
```cpp
class database_base {
    virtual database_types database_type() = 0;
    virtual bool connect(const std::string& connect_string) = 0;
    virtual bool disconnect() = 0;
    virtual bool create_query(const std::string& query) = 0;
    virtual unsigned int insert_query(const std::string& query) = 0;
    virtual unsigned int update_query(const std::string& query) = 0;
    virtual unsigned int delete_query(const std::string& query) = 0;
    virtual database_result select_query(const std::string& query) = 0;
    virtual bool execute_query(const std::string& query) = 0;
};
```

**database_backend (Modern):**
```cpp
class database_backend {
    virtual database_types type() const = 0;
    virtual VoidResult initialize(const connection_config& config) = 0;
    virtual VoidResult shutdown() = 0;
    virtual bool is_initialized() const = 0;
    virtual Result<uint64_t> insert_query(const std::string& query) = 0;
    virtual Result<uint64_t> update_query(const std::string& query) = 0;
    virtual Result<uint64_t> delete_query(const std::string& query) = 0;
    virtual Result<database_result> select_query(const std::string& query) = 0;
    virtual VoidResult execute_query(const std::string& query) = 0;
    virtual VoidResult begin_transaction() = 0;
    virtual VoidResult commit_transaction() = 0;
    virtual VoidResult rollback_transaction() = 0;
    virtual bool in_transaction() const = 0;
    virtual std::string last_error() const = 0;
    virtual std::map<std::string, std::string> connection_info() const = 0;
};
```

### Step 2: Update Includes and Types

**Before:**
```cpp
#include "database/database_base.h"

std::shared_ptr<database::database_base> db;
```

**After:**
```cpp
#include "database/core/database_backend.h"
#include "database/core/backend_registry.h"

std::unique_ptr<database::core::database_backend> backend;
```

### Step 3: Update Connection Code

**Before:**
```cpp
auto db = std::make_shared<database::postgres_manager>();
if (!db->connect("host=localhost dbname=mydb user=user password=pass")) {
    // Error, but no details available
    std::cerr << "Connection failed" << std::endl;
}
```

**After:**
```cpp
auto backend = database::core::backend_registry::instance().create("postgresql");

database::core::connection_config config;
config.host = "localhost";
config.database = "mydb";
config.username = "user";
config.password = "pass";

auto result = backend->initialize(config);
if (result.is_err()) {
    std::cerr << "Connection failed: " << result.error().message << std::endl;
}
```

### Step 4: Update Query Execution

**Before:**
```cpp
// INSERT - can't distinguish error from 0 rows affected
auto rows = db->insert_query("INSERT INTO users (name) VALUES ('John')");
if (rows == 0) {
    // Was it an error or just no rows affected?
}

// SELECT - empty result could be error or no data
auto data = db->select_query("SELECT * FROM users");
if (data.empty()) {
    // Was it an error or just no results?
}
```

**After:**
```cpp
// INSERT - clear distinction between error and 0 rows
auto insert_result = backend->insert_query("INSERT INTO users (name) VALUES ('John')");
if (insert_result.is_err()) {
    std::cerr << "Insert failed: " << insert_result.error().message << std::endl;
} else {
    std::cout << "Inserted " << insert_result.value() << " rows" << std::endl;
}

// SELECT - explicit error handling
auto select_result = backend->select_query("SELECT * FROM users");
if (select_result.is_err()) {
    std::cerr << "Select failed: " << select_result.error().message << std::endl;
} else {
    auto& data = select_result.value();
    if (data.empty()) {
        std::cout << "No results found" << std::endl;
    }
}
```

### Step 5: Add Transaction Support

**Before (not supported):**
```cpp
// Transactions were not supported in database_base
db->execute_query("BEGIN");
db->insert_query("INSERT INTO users (name) VALUES ('John')");
db->execute_query("COMMIT");
```

**After:**
```cpp
// Native transaction support
if (auto result = backend->begin_transaction(); result.is_err()) {
    std::cerr << "Failed to begin transaction: " << result.error().message << std::endl;
    return;
}

auto insert_result = backend->insert_query("INSERT INTO users (name) VALUES ('John')");
if (insert_result.is_err()) {
    backend->rollback_transaction();
    std::cerr << "Insert failed: " << insert_result.error().message << std::endl;
    return;
}

if (auto result = backend->commit_transaction(); result.is_err()) {
    std::cerr << "Commit failed: " << result.error().message << std::endl;
}
```

### Step 6: Update Disconnection Code

**Before:**
```cpp
if (!db->disconnect()) {
    // Error, but no details
}
```

**After:**
```cpp
auto result = backend->shutdown();
if (result.is_err()) {
    std::cerr << "Shutdown failed: " << result.error().message << std::endl;
}
```

## Gradual Migration with Adapter

If you have a large codebase and can't migrate all at once, use `database_base_adapter`:

```cpp
#include "database/database_base_adapter.h"
#include "database/core/backend_registry.h"

// Create new backend
auto backend = database::core::backend_registry::instance().create("postgresql");

// Wrap it for legacy code compatibility
auto adapter = std::make_shared<database::database_base_adapter>(std::move(backend));

// Use adapter with legacy code expecting database_base
legacy_function(adapter);

// Access underlying backend when needed
const auto& underlying = adapter->backend();
std::cout << "Last error: " << underlying.last_error() << std::endl;
```

## Method Mapping Reference

| database_base | database_backend | Notes |
|---------------|------------------|-------|
| `database_type()` | `type()` | Same functionality |
| `connect(string)` | `initialize(connection_config)` | Structured config |
| `disconnect()` | `shutdown()` | Returns `VoidResult` |
| `create_query(string)` | `execute_query(string)` | Same functionality |
| `insert_query(string)` | `insert_query(string)` | Returns `Result<uint64_t>` |
| `update_query(string)` | `update_query(string)` | Returns `Result<uint64_t>` |
| `delete_query(string)` | `delete_query(string)` | Returns `Result<uint64_t>` |
| `select_query(string)` | `select_query(string)` | Returns `Result<database_result>` |
| `execute_query(string)` | `execute_query(string)` | Returns `VoidResult` |
| N/A | `begin_transaction()` | New feature |
| N/A | `commit_transaction()` | New feature |
| N/A | `rollback_transaction()` | New feature |
| N/A | `in_transaction()` | New feature |
| N/A | `is_initialized()` | New feature |
| N/A | `last_error()` | New feature |
| N/A | `connection_info()` | New feature |

## Error Handling Migration

### Result Type Usage

```cpp
// Check for success
if (result.is_ok()) {
    auto value = result.value();
}

// Check for error
if (result.is_err()) {
    auto& error = result.error();
    std::cerr << "Error code: " << error.code << std::endl;
    std::cerr << "Message: " << error.message << std::endl;
    std::cerr << "Module: " << error.module << std::endl;
}

// Using value_or for defaults
auto count = result.value_or(0);

// Using map for transformations
auto doubled = result.map([](auto v) { return v * 2; });
```

## Removed Features

The following `database_base` features are no longer directly available:

| Feature | Status | Alternative |
|---------|--------|-------------|
| `create_query()` | Removed | Use `execute_query()` for DDL |

## Troubleshooting

### Compilation Error: deprecated declaration

```
warning: 'database_base' is deprecated
```

**Solution:** Update your code to use `database_backend` or `database_base_adapter`.

### Runtime Error: backend not found

```
Error: Backend 'postgresql' not registered
```

**Solution:** Ensure the backend is registered. Link against the appropriate backend library.

### Type Mismatch: Result vs raw value

```
error: cannot convert 'Result<uint64_t>' to 'unsigned int'
```

**Solution:** Use `.value()` to extract the value after checking `.is_ok()`.

## Migration Timeline

| Version | Status | Action |
|---------|--------|--------|
| v0.4.0.0 | Current | `database_base` deprecated, adapter available |
| v0.4.x | Planned | Assist migration, monitor usage |
| v0.5.0.0 | Planned | `database_base` removed |

## Related Documentation

- [API Reference](../API_REFERENCE.md)
- [Architecture Overview](../ARCHITECTURE.md)
- [Backend Registry Guide](../guides/backend_registry.md)

---

*Last updated: v0.4.0.0 - Phase 1 (Deprecation)*
