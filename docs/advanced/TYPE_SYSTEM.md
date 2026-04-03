---
doc_id: "DBS-API-003"
doc_title: "Database System Type System Documentation"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "API"
---

# Database System Type System Documentation

**Version**: 0.1.0.0
**Created**: 2025-11-11  
**Last Updated**: 2025-11-11

---

## 📋 Overview

The database_system uses a flexible type system to handle values across different database backends (PostgreSQL, SQLite, MongoDB, Redis). This document explains when to use each type, how to convert between them, best practices, and performance implications.

---

## 🏗️ Core Type System

### 1. `database_value` (Variant-Based)

**Purpose**: Universal value container for all database types

**Definition**:
```cpp
using database_value = std::variant<
    std::monostate,  // NULL
    std::string,
    int64_t,
    double,
    bool
>;
```

**When to Use**:
- ✅ Passing parameters to queries
- ✅ Receiving results from queries
- ✅ Building dynamic query conditions
- ✅ ORM entity field values

**Example**:
```cpp
database_value name = "Alice";
database_value age = 30;
database_value salary = 50000.0;
database_value is_active = true;
database_value null_value = std::monostate{};
```

**Performance**:
- Memory: 32 bytes (16 bytes discriminator + 16 bytes value)
- Access: O(1) with `std::visit()` or `std::holds_alternative()`
- Type-safe: Compile-time type checking

---

### 2. `database_row` (Map-Based)

**Purpose**: Represents a single row from query results

**Definition**:
```cpp
using database_row = std::map<std::string, database_value>;
```

**When to Use**:
- ✅ Processing query results
- ✅ Building INSERT/UPDATE data
- ✅ Dynamic schema handling

**Example**:
```cpp
database_row row;
row["id"] = 1;
row["name"] = "Bob";
row["created_at"] = "2025-11-11";

// Access values
if (auto* name = std::get_if<std::string>(&row["name"])) {
    std::cout << "Name: " << *name << "\n";
}
```

**Performance**:
- Lookup: O(log n) for column access
- Memory: ~48 bytes overhead per column (map node + string key)
- Trade-off: Flexibility vs. speed

---

### 3. `Result<T>` (Error Handling)

**Purpose**: Type-safe error handling without exceptions

**When `KCENON_HAS_COMMON_SYSTEM` is enabled**:
```cpp
#include <kcenon/common/patterns/result.h>
using common::Result;
```

**When standalone** (fallback):
```cpp
// database/core/result.h
template<typename T>
class Result {
    std::variant<T, error_info> value_;
public:
    bool is_ok() const;
    bool is_error() const;
    const T& value() const;
    const error_info& error() const;
};
```

**When to Use**:
- ✅ Any operation that can fail
- ✅ Database connections
- ✅ Query execution
- ✅ Transaction management

**Example**:
```cpp
Result<database_row> result = db.query_single("SELECT * FROM users WHERE id = 1");

if (result.is_ok()) {
    const auto& row = result.value();
    // Process row
} else {
    std::cerr << "Error: " << result.error().message << "\n";
}
```

**Performance**:
- Zero-cost abstraction when successful
- No heap allocation
- Inlined by compiler

---

### 4. Backend-Specific Types

#### PostgreSQL
```cpp
// Uses libpq types internally
PGresult* pg_result = ...;
database_row row = convert_postgres_row(pg_result, row_index);
```


#### SQLite
```cpp
// Uses sqlite3_stmt* internally
sqlite3_stmt* stmt = ...;
database_row row = convert_sqlite_row(stmt);
```

#### MongoDB
```cpp
// Uses BSON documents
bson_t* bson_doc = ...;
database_row row = convert_bson_to_row(bson_doc);
```

#### Redis
```cpp
// Uses redisReply*
redisReply* reply = ...;
database_value value = convert_redis_reply(reply);
```

---

## 🔄 Type Conversion Utilities

### 1. `database_value` → String

```cpp
std::string value_to_string(const database_value& value) {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else {
            return "NULL";
        }
    }, value);
}
```

### 2. String → `database_value`

```cpp
database_value string_to_value(const std::string& str, const std::string& type_hint) {
    if (type_hint == "integer") {
        return std::stoll(str);
    } else if (type_hint == "real") {
        return std::stod(str);
    } else if (type_hint == "boolean") {
        return str == "true" || str == "1";
    } else {
        return str;  // Default to string
    }
}
```

### 3. Backend Result → `database_row`

Each backend implements a conversion function:

```cpp
database_row convert_backend_result(backend_result_type result);
```

---

## 📊 Best Practices

### 1. **Prefer `database_value` for Parameters**

✅ **Good**:
```cpp
auto result = db.execute(
    "SELECT * FROM users WHERE age > ? AND active = ?",
    database_value{18},
    database_value{true}
);
```

❌ **Avoid**:
```cpp
// String concatenation risks SQL injection
auto result = db.execute(
    "SELECT * FROM users WHERE age > " + std::to_string(age)
);
```

### 2. **Use `std::visit()` for Type-Safe Access**

✅ **Good**:
```cpp
std::visit([](const auto& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::string>) {
        std::cout << "String: " << value << "\n";
    } else if constexpr (std::is_same_v<T, int64_t>) {
        std::cout << "Integer: " << value << "\n";
    }
}, db_value);
```

❌ **Avoid**:
```cpp
// Throws std::bad_variant_access if wrong type
auto str = std::get<std::string>(db_value);
```

### 3. **Handle NULL Values Explicitly**

✅ **Good**:
```cpp
if (std::holds_alternative<std::monostate>(row["optional_field"])) {
    // Handle NULL case
    std::cout << "Field is NULL\n";
} else {
    // Process value
}
```

### 4. **Reuse `database_row` for Batch Operations**

✅ **Good**:
```cpp
database_row row;
std::vector<database_row> batch;

for (const auto& user : users) {
    row.clear();  // Reuse existing map
    row["name"] = user.name;
    row["age"] = user.age;
    batch.push_back(row);
}

db.batch_insert("users", batch);
```

### 5. **Use Query Builder for Complex Queries**

✅ **Good**:
```cpp
auto builder = db.create_query_builder();
builder.select("name, email")
    .from("users")
    .where("age", ">", 18)
    .where("active", "=", true)
    .order_by("name")
    .limit(100);

auto result = db.execute(builder.build());
```

---

## ⚡ Performance Implications

### Memory Usage

| Type | Size (bytes) | Heap Allocations |
|------|--------------|------------------|
| `database_value` | 32 | 1 (for std::string only) |
| `database_row` (5 cols) | ~240 | 6 (map nodes + strings) |
| `Result<T>` | sizeof(T) + 32 | 0 (stack-only) |

### Access Performance

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| `database_value` type check | O(1) | `std::holds_alternative()` |
| `database_value` access | O(1) | `std::get_if()` or `std::visit()` |
| `database_row` lookup | O(log n) | `std::map` lookup |
| `Result<T>` check | O(1) | `is_ok()` / `is_error()` |

### Optimization Tips

1. **Avoid repeated `database_row` lookups**:
   ```cpp
   // ❌ Multiple lookups
   process(row["name"], row["email"], row["name"]);
   
   // ✅ Cache reference
   const auto& name = row["name"];
   process(name, row["email"], name);
   ```

2. **Use move semantics for large values**:
   ```cpp
   database_row row;
   row["large_text"] = std::move(large_string);  // No copy
   ```

3. **Reserve capacity for batch operations**:
   ```cpp
   std::vector<database_row> rows;
   rows.reserve(1000);  // Avoid reallocations
   ```

---

## 🧩 ORM Integration

### Entity Field Mapping

```cpp
class user_entity {
    int64_t id_;
    std::string name_;
    bool active_;

public:
    void from_row(const database_row& row) {
        id_ = std::get<int64_t>(row.at("id"));
        name_ = std::get<std::string>(row.at("name"));
        active_ = std::get<bool>(row.at("active"));
    }

    database_row to_row() const {
        return {
            {"id", id_},
            {"name", name_},
            {"active", active_}
        };
    }
};
```

---

## 🔍 Debugging Type Issues

### Common Errors

#### 1. **`std::bad_variant_access`**

**Cause**: Accessing wrong type from `database_value`

**Solution**: Use `std::get_if()` or `std::visit()`

```cpp
// ❌ Throws if not int64_t
auto age = std::get<int64_t>(row["age"]);

// ✅ Safe access
if (auto* age = std::get_if<int64_t>(&row["age"])) {
    std::cout << "Age: " << *age << "\n";
} else {
    std::cerr << "Age is not an integer\n";
}
```

#### 2. **`std::out_of_range`**

**Cause**: Accessing non-existent column in `database_row`

**Solution**: Use `.find()` or `.contains()` (C++20)

```cpp
// ❌ Throws if "optional_field" doesn't exist
auto value = row.at("optional_field");

// ✅ Safe access
if (auto it = row.find("optional_field"); it != row.end()) {
    auto value = it->second;
}
```

---

## 📚 Related Documentation

- [API Reference](API_REFERENCE.md)
- [Query Builder Guide](../database/query_builder.h)
- [ORM Guide](ORM.md)
- [Backend Integration](BACKEND_INTEGRATION.md)

---

## ✅ Summary

### Quick Reference

| Use Case | Type | Performance |
|----------|------|-------------|
| Query parameters | `database_value` | Excellent (stack-only) |
| Query results | `database_row` | Good (O(log n) lookup) |
| Error handling | `Result<T>` | Excellent (zero-cost) |
| Batch operations | `std::vector<database_row>` | Good (reserve capacity) |
| ORM entities | Custom + `to_row()`/`from_row()` | Excellent (type-safe) |

### Key Takeaways

1. ✅ Use `database_value` for type-safe value handling
2. ✅ Use `database_row` for flexible row representation
3. ✅ Use `Result<T>` for error handling without exceptions
4. ✅ Use `std::visit()` or `std::get_if()` for safe type access
5. ✅ Handle NULL values explicitly with `std::monostate`
6. ✅ Use query builder for complex queries
7. ✅ Profile and optimize hot paths

---

**Maintained by**: database_system maintainers  
**Review Cycle**: Quarterly  
**Next Review**: 2025-Q2
