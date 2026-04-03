---
doc_id: "DBS-GUID-023"
doc_title: "Migrating from Legacy Bool-Returning API to Result-Based API"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Migrating from Legacy Bool-Returning API to Result-Based API

> **SSOT**: This document is the single source of truth for **Migrating from Legacy Bool-Returning API to Result-Based API**.

## Overview

The `database_manager` class previously had two sets of API methods:
1. **Legacy API**: Methods returning `bool` or `unsigned int`
2. **Result-based API**: Methods returning `Result<T>` or `VoidResult`

As of Issue #294, the legacy bool-returning methods are deprecated. This guide helps you migrate to the Result-based API.

## Timeline

- **v1.x**: Legacy methods deprecated with `[[deprecated]]` attribute
- **v2.0.0**: Legacy methods will be removed (planned)

## Why Migrate?

| Aspect | Legacy API | Result-based API |
|--------|-----------|-----------------|
| Error details | No context (just `false`) | Full error message and code |
| Error handling | Manual, error-prone | Type-safe, explicit |
| Debugging | Difficult | Clear error information |
| Code clarity | Ambiguous failure | Self-documenting |

## Method Mapping

| Legacy Method (deprecated) | Result-based Method |
|---------------------------|---------------------|
| `connect(string)` → `bool` | `connect_result(string)` → `VoidResult` |
| `disconnect()` → `bool` | `disconnect_result()` → `VoidResult` |
| `create_query(string)` → `bool` | `create_query_result(string)` → `VoidResult` |
| `insert_query(string)` → `unsigned int` | `insert_query_result(string)` → `Result<uint64_t>` |
| `update_query(string)` → `unsigned int` | `update_query_result(string)` → `Result<uint64_t>` |
| `delete_query(string)` → `unsigned int` | `delete_query_result(string)` → `Result<uint64_t>` |
| `select_query(string)` → `database_result` | `select_query_result(string)` → `Result<database_result>` |

## Migration Examples

### Connection

```cpp
// Before (deprecated)
auto db = std::make_shared<database_manager>(context);
db->set_mode(database_types::postgres);
if (!db->connect(conn_string)) {
    std::cerr << "Connection failed" << std::endl;  // No error details
    return false;
}

// After
auto db = std::make_shared<database_manager>(context);
db->set_mode(database_types::postgres);
auto result = db->connect_result(conn_string);
if (result.is_err()) {
    std::cerr << "Connection failed: " << result.error().message << std::endl;
    std::cerr << "Error code: " << result.error().code << std::endl;
    return result.error();
}
```

### INSERT Query

```cpp
// Before (deprecated)
unsigned int rows = db->insert_query("INSERT INTO users (name) VALUES ('John')");
if (rows == 0) {
    std::cerr << "Insert failed" << std::endl;  // Was it 0 rows or error?
}

// After
auto result = db->insert_query_result("INSERT INTO users (name) VALUES ('John')");
if (result.is_err()) {
    std::cerr << "Insert failed: " << result.error().message << std::endl;
    return result.error();
}
uint64_t rows = result.value();
std::cout << "Inserted " << rows << " rows" << std::endl;
```

### SELECT Query

```cpp
// Before (deprecated)
auto results = db->select_query("SELECT * FROM users");
if (results.empty()) {
    // Is it empty result or query error?
    std::cerr << "No results or error" << std::endl;
}

// After
auto result = db->select_query_result("SELECT * FROM users");
if (result.is_err()) {
    std::cerr << "Query failed: " << result.error().message << std::endl;
    return result.error();
}

auto& rows = result.value();
if (rows.empty()) {
    std::cout << "No results found" << std::endl;  // Clearly: query OK, no data
}
```

### Disconnection

```cpp
// Before (deprecated)
if (!db->disconnect()) {
    std::cerr << "Disconnect failed" << std::endl;
}

// After
auto result = db->disconnect_result();
if (result.is_err()) {
    std::cerr << "Disconnect failed: " << result.error().message << std::endl;
}
```

## Working with Result Types

### Basic Pattern

```cpp
auto result = db->some_operation();

// Check success
if (result.is_ok()) {
    auto value = result.value();  // Access the value
}

// Check failure
if (result.is_err()) {
    auto error = result.error();
    std::cerr << error.message << std::endl;
    std::cerr << "Code: " << error.code << std::endl;
}
```

### Using value_or() for Defaults

```cpp
auto result = db->select_query_result("SELECT * FROM users");
auto rows = result.value_or(database_result{});  // Empty result on error
```

### Chaining Operations

```cpp
auto connect_result = db->connect_result(conn_string);
if (connect_result.is_err()) {
    return connect_result.error();
}

auto create_result = db->create_query_result("CREATE TABLE IF NOT EXISTS...");
if (create_result.is_err()) {
    db->disconnect_result();  // Clean up
    return create_result.error();
}
```

## Transaction Support

The Result-based API also provides transaction support:

```cpp
// Begin transaction
auto begin_result = db->begin_transaction();
if (begin_result.is_err()) {
    return begin_result.error();
}

// Execute queries
auto insert_result = db->insert_query_result("INSERT INTO...");
if (insert_result.is_err()) {
    db->rollback_transaction();
    return insert_result.error();
}

// Commit on success
auto commit_result = db->commit_transaction();
if (commit_result.is_err()) {
    db->rollback_transaction();
    return commit_result.error();
}
```

## Suppressing Deprecation Warnings

If you need to temporarily suppress deprecation warnings during migration:

```cpp
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // Disable deprecated warning
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

// Legacy code here
db->connect(conn_string);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
```

**Note**: This is only recommended for gradual migration. All code should use Result-based API before v2.0.0.

## See Also

- [API Reference](../API_REFERENCE.md) - Complete API documentation
- [database_base Migration](database_base.md) - Migrating from database_base to database_backend
- [Architecture](../ARCHITECTURE.md) - System architecture overview
