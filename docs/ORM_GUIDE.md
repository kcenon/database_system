# ORM Framework Guide

> **Status**: Full Support (C++17 SFINAE-based)
> **Header**: `database/orm/entity.h`
> **Namespace**: `database::orm`

This document provides comprehensive documentation for the database_system ORM (Object-Relational Mapping) framework, including entity definition, macro system, CRUD operations, query building, and schema management.

## Table of Contents

- [Overview](#overview)
  - [Design Philosophy](#design-philosophy)
  - [Supported Backends](#supported-backends)
  - [When to Use Raw SQL Instead](#when-to-use-raw-sql-instead)
- [Entity Definition](#entity-definition)
  - [ENTITY\_TABLE Macro](#entity_table-macro)
  - [ENTITY\_FIELD Macro](#entity_field-macro)
  - [ENTITY\_METADATA Macro](#entity_metadata-macro)
  - [Complete Entity Example](#complete-entity-example)
- [Supported Types and Type Mapping](#supported-types-and-type-mapping)
- [Field Constraints](#field-constraints)
  - [Constraint Functions](#constraint-functions)
  - [Combining Constraints](#combining-constraints)
- [CRUD Operations](#crud-operations)
  - [Create (INSERT)](#create-insert)
  - [Read (SELECT)](#read-select)
  - [Update](#update)
  - [Delete](#delete)
- [Query Builder](#query-builder)
  - [Filtering (where)](#filtering-where)
  - [Ordering (order\_by)](#ordering-order_by)
  - [Pagination (limit/offset)](#pagination-limitoffset)
  - [Joins](#joins)
  - [Aggregations](#aggregations)
  - [Execution Methods](#execution-methods)
- [Schema Management](#schema-management)
  - [Entity Registration](#entity-registration)
  - [Table Creation](#table-creation)
  - [Schema Synchronization](#schema-synchronization)
- [Metadata System](#metadata-system)
  - [field\_metadata](#field_metadata)
  - [entity\_metadata](#entity_metadata)
  - [entity\_manager](#entity_manager)
- [Complete Examples](#complete-examples)
  - [User Entity](#user-entity)
  - [Blog Post with Foreign Key](#blog-post-with-foreign-key)
- [Limitations](#limitations)
- [Related Documentation](#related-documentation)

---

## Overview

### Design Philosophy

The database_system ORM follows the **Active Record** pattern, where each entity object corresponds to a row in the database table and encapsulates both data and database operations.

```
C++ Class (Entity)          ←→    Database Table
├── Fields (members)        ←→    Columns
├── save() / load()         ←→    INSERT / SELECT
├── update() / remove()     ←→    UPDATE / DELETE
└── Metadata                ←→    Schema definition
```

Key design principles:
- **Declarative mapping**: Use macros to define entity-to-table relationships
- **Type safety**: SFINAE-based compile-time checks ensure only valid types are used as fields
- **Constraint composition**: Combine field constraints using the `|` operator (bitwise OR)
- **Zero-cost abstractions**: Metadata is computed once via lazy initialization

### Supported Backends

The ORM works with all `database_backend` implementations:

| Backend | ORM Support | Notes |
|---------|-------------|-------|
| PostgreSQL | Full | Recommended for production |
| MySQL | Full | |
| SQLite | Full | Good for development/testing |
| MongoDB | Limited | Relational features may not apply |
| Redis | Limited | Key-value model differs from relational |

### When to Use Raw SQL Instead

The ORM is best for standard CRUD operations. Consider raw SQL for:

- Complex multi-table joins with subqueries
- Database-specific features (PostgreSQL JSONB operators, window functions)
- Bulk operations requiring maximum performance
- Stored procedure calls
- Schema migrations with data transformation

---

## Entity Definition

An entity is defined by combining three macros within a class that inherits from `entity_base`:

```cpp
#include <database/orm/entity.h>
using namespace database::orm;

class MyEntity : public entity_base {
    ENTITY_TABLE("my_table")          // 1. Map class to table

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())  // 2. Define fields
    ENTITY_FIELD(std::string, name, not_null())

    ENTITY_METADATA()                 // 3. Enable metadata system
};
```

### ENTITY_TABLE Macro

```cpp
ENTITY_TABLE("table_name")
```

Maps the entity class to a database table.

**What it generates**:
- `table_name()` override returning the specified table name string
- `primary_key_type` type alias (aliased to `decltype(id_)`, so an `id` field must be defined)
- Static `entity_metadata` instance for the table

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| `table_name` | string literal | The database table name |

**Rules**:
- Must appear once per entity class
- Must be placed before any `ENTITY_FIELD` definitions that reference `id`
- The entity must define a field named `id` (used for `primary_key_type`)

### ENTITY_FIELD Macro

```cpp
ENTITY_FIELD(type, name, constraints...)
```

Maps a C++ member variable to a database column.

**What it generates**:
- Private member `name_` of the specified type
- Static `field_metadata` instance `name_metadata_`
- Public `field_accessor<type>` named `name` for type-safe access
- Static method `name_field()` returning the field's metadata

**Parameters**:
| Parameter | Type | Description |
|-----------|------|-------------|
| `type` | C++ type | The field's C++ type (must be a supported type) |
| `name` | identifier | The field name (becomes both member name and column name) |
| `constraints...` | `field_constraint` | Optional constraints (variadic, combined with `\|`) |

**Example**:
```cpp
ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
ENTITY_FIELD(std::string, username, not_null() | unique() | index("idx_username"))
ENTITY_FIELD(std::string, email, not_null())
ENTITY_FIELD(bool, is_active)  // No constraints
```

### ENTITY_METADATA Macro

```cpp
ENTITY_METADATA()
```

Enables the metadata system for the entity with lazy initialization.

**What it generates**:
- `get_metadata()` override with lazy initialization (initialized once on first access; not thread-safe — call from a single thread or synchronize externally)
- Static `initialize_metadata()` declaration (must be implemented by the user)

**Required implementation**: You must implement `initialize_metadata()` outside the class:

```cpp
void MyEntity::initialize_metadata() {
    metadata_.add_field(id_field());
    metadata_.add_field(name_field());
    // Add all fields in order
}
```

### Complete Entity Example

```cpp
#include <database/orm/entity.h>
using namespace database::orm;

class User : public entity_base {
    ENTITY_TABLE("users")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique())
    ENTITY_FIELD(std::string, email, not_null())
    ENTITY_FIELD(std::string, role, not_null())
    ENTITY_FIELD(bool, is_active, not_null())
    ENTITY_FIELD(std::chrono::system_clock::time_point, created_at, default_now())

    ENTITY_METADATA()

public:
    User() {
        is_active = true;
        created_at = std::chrono::system_clock::now();
    }
};

// Implement metadata initialization
void User::initialize_metadata() {
    metadata_.add_field(id_field());
    metadata_.add_field(username_field());
    metadata_.add_field(email_field());
    metadata_.add_field(role_field());
    metadata_.add_field(is_active_field());
    metadata_.add_field(created_at_field());
}
```

---

## Supported Types and Type Mapping

The `is_field_type_v<T>` trait validates types at compile time. Only these types are accepted:

| C++ Type | SQL Type | Notes |
|----------|----------|-------|
| `int32_t` | `INTEGER` | 32-bit integer |
| `int64_t` | `BIGINT` | 64-bit integer (recommended for IDs) |
| `double` | `DOUBLE PRECISION` | Floating-point |
| `std::string` | `VARCHAR(255)` | Text data |
| `bool` | `BOOLEAN` | True/false |
| `std::chrono::system_clock::time_point` | `TIMESTAMP` | Date/time |

Any other type used in `ENTITY_FIELD` will cause a compile-time error via SFINAE.

---

## Field Constraints

### Constraint Functions

| Function | `field_constraint` Value | SQL Effect |
|----------|--------------------------|------------|
| `primary_key()` | `primary_key` (1) | `PRIMARY KEY` |
| `not_null()` | `not_null` (2) | `NOT NULL` |
| `unique()` | `unique` (4) | `UNIQUE` |
| `auto_increment()` | `auto_increment` (8) | `AUTO_INCREMENT` |
| `index("name")` | `index` (16) | Creates named index |
| `foreign_key("table", "field")` | `foreign_key` (32) | `FOREIGN KEY ... REFERENCES` |
| `default_now()` | `default_now` (64) | `DEFAULT CURRENT_TIMESTAMP` |

### Combining Constraints

Constraints use bitwise OR (`|`) for composition:

```cpp
// Primary key with auto-increment
ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())

// Non-null, unique, and indexed
ENTITY_FIELD(std::string, email, not_null() | unique() | index("idx_email"))

// Foreign key with not-null
ENTITY_FIELD(int64_t, user_id, foreign_key("users", "id") | not_null())

// Timestamp with default value
ENTITY_FIELD(std::chrono::system_clock::time_point, created_at, default_now())
```

You can check constraints programmatically:

```cpp
const auto& meta = User::id_field();
meta.is_primary_key();    // true
meta.is_auto_increment(); // true
meta.is_not_null();       // false (primary_key implies NOT NULL)
meta.is_unique();         // false
```

---

## CRUD Operations

### Create (INSERT)

```cpp
User user;
user.username = "john_doe";
user.email = "john@example.com";
user.role = "admin";

bool success = user.save();
```

### Read (SELECT)

Use the query builder for type-safe reads:

```cpp
auto entity_mgr = context->get_entity_manager();

// Query all active users
auto active_users = entity_mgr.query<User>(db)
    .where("is_active = true")
    .order_by("username")
    .execute();

for (const auto& user : active_users) {
    std::cout << user.username.get() << std::endl;
}

// Get first matching result
auto admin = entity_mgr.query<User>(db)
    .where("role = 'admin'")
    .first();

if (admin.has_value()) {
    std::cout << "Admin: " << admin->username.get() << std::endl;
}
```

### Update

```cpp
User user;
user.load();  // Load existing data
user.email = "newemail@example.com";
bool success = user.update();
```

### Delete

```cpp
User user;
user.load();
bool success = user.remove();
```

---

## Query Builder

The `query_builder<EntityType>` provides a fluent API for building type-safe queries. It is obtained via `entity_manager::query<T>()`.

```cpp
auto results = entity_mgr.query<User>(db)
    .where("is_active = true")
    .order_by("created_at", false)  // descending
    .limit(10)
    .offset(20)
    .execute();
```

### Filtering (where)

```cpp
// Simple condition
.where("is_active = true")

// Multiple where clauses (AND)
.where("is_active = true")
.where("role = 'admin'")
```

### Ordering (order_by)

```cpp
// Ascending (default)
.order_by("username")
.order_by("username", true)

// Descending
.order_by("created_at", false)
```

### Pagination (limit/offset)

```cpp
// First 10 results
.limit(10)

// Skip 20, take 10 (page 3 with page size 10)
.offset(20)
.limit(10)
```

### Joins

```cpp
// Inner join
.join<Post>("users.id = posts.user_id")

// Left join
.left_join<Post>("users.id = posts.user_id")
```

Join methods use SFINAE to ensure `OtherEntity` is a valid entity type.

### Aggregations

```cpp
// Count matching rows
size_t total = entity_mgr.query<User>(db)
    .where("is_active = true")
    .count();

// Sum a numeric field
double total_amount = entity_mgr.query<Order>(db)
    .where("status = 'completed'")
    .sum("amount");

// Average
double avg_amount = entity_mgr.query<Order>(db)
    .avg("amount");

// Min/Max
auto min_val = entity_mgr.query<Order>(db).min("amount");
auto max_val = entity_mgr.query<Order>(db).max("amount");
```

### Execution Methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `execute()` | `std::vector<EntityType>` | Returns all matching entities |
| `first()` | `std::optional<EntityType>` | Returns first match or `nullopt` |
| `count()` | `size_t` | Returns count of matching rows |
| `sum(field)` | `double` | Sum of a numeric field |
| `avg(field)` | `double` | Average of a numeric field |
| `min(field)` | `database_value` | Minimum value |
| `max(field)` | `database_value` | Maximum value |

---

## Schema Management

### Entity Registration

Before using an entity, register it with the `entity_manager`:

```cpp
auto context = std::make_shared<database_context>();
auto& entity_mgr = context->get_entity_manager();

entity_mgr.register_entity<User>();
entity_mgr.register_entity<Post>();
```

### Table Creation

Create all registered entity tables:

```cpp
// Create tables (CREATE TABLE IF NOT EXISTS)
bool success = entity_mgr.create_tables(db);

// Drop all registered tables
bool dropped = entity_mgr.drop_tables(db);
```

The `create_tables()` method generates SQL from entity metadata:

```sql
CREATE TABLE IF NOT EXISTS users (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(255) NOT NULL UNIQUE,
  email VARCHAR(255) NOT NULL,
  role VARCHAR(255) NOT NULL,
  is_active BOOLEAN NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)
```

Indexes are created separately:

```sql
CREATE INDEX IF NOT EXISTS idx_username ON users(username);
```

### Schema Synchronization

The `sync_schema()` method drops and recreates all tables:

```cpp
// WARNING: Destroys existing data
bool synced = entity_mgr.sync_schema(db);
```

> **Note**: `sync_schema()` currently performs a destructive drop-and-recreate. It does not perform incremental schema diffing. Use with caution in production.

---

## Metadata System

### field_metadata

Stores metadata for a single entity field.

| Method | Return Type | Description |
|--------|-------------|-------------|
| `name()` | `const std::string&` | Column name |
| `type_name()` | `const std::string&` | C++ type name as string |
| `constraints()` | `field_constraint` | Bitfield of constraints |
| `index_name()` | `const std::string&` | Named index (if any) |
| `foreign_table()` | `const std::string&` | Referenced table for FK |
| `foreign_field()` | `const std::string&` | Referenced field for FK |
| `is_primary_key()` | `bool` | Check primary key constraint |
| `is_not_null()` | `bool` | Check not-null constraint |
| `is_unique()` | `bool` | Check unique constraint |
| `is_auto_increment()` | `bool` | Check auto-increment |
| `has_index()` | `bool` | Check if indexed |
| `is_foreign_key()` | `bool` | Check foreign key |
| `has_default_now()` | `bool` | Check default timestamp |
| `to_sql_definition()` | `std::string` | Generate SQL column definition |

### entity_metadata

Stores metadata for an entire entity (table).

| Method | Return Type | Description |
|--------|-------------|-------------|
| `table_name()` | `const std::string&` | Table name |
| `fields()` | `const vector<field_metadata>&` | All field metadata |
| `get_primary_key()` | `const field_metadata*` | Primary key field (or nullptr) |
| `get_indexes()` | `vector<const field_metadata*>` | All indexed fields |
| `get_foreign_keys()` | `vector<const field_metadata*>` | All foreign key fields |
| `create_table_sql()` | `std::string` | Generate CREATE TABLE SQL |
| `create_indexes_sql()` | `std::string` | Generate CREATE INDEX SQL |

### entity_manager

Manages entity registration and provides factory methods.

| Method | Description |
|--------|-------------|
| `register_entity<T>()` | Register an entity type |
| `get_metadata<T>()` | Get metadata for a registered entity |
| `query<T>(db)` | Create a query builder for the entity type |
| `create_tables(db)` | Create tables for all registered entities |
| `drop_tables(db)` | Drop tables for all registered entities |
| `sync_schema(db)` | Drop and recreate all tables |

---

## Complete Examples

### User Entity

```cpp
#include <database/orm/entity.h>
#include <database/database_manager.h>
#include <database/core/database_context.h>

using namespace database;
using namespace database::orm;

// 1. Define entity
class User : public entity_base {
    ENTITY_TABLE("users")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique() | index("idx_username"))
    ENTITY_FIELD(std::string, email, not_null() | unique())
    ENTITY_FIELD(bool, is_active, not_null())
    ENTITY_FIELD(std::chrono::system_clock::time_point, created_at, default_now())

    ENTITY_METADATA()

public:
    User() { is_active = true; }
};

void User::initialize_metadata() {
    metadata_.add_field(id_field());
    metadata_.add_field(username_field());
    metadata_.add_field(email_field());
    metadata_.add_field(is_active_field());
    metadata_.add_field(created_at_field());
}

// 2. Use entity
void example_usage() {
    auto context = std::make_shared<database_context>();
    auto db_mgr = std::make_shared<database_manager>(context);
    auto& entity_mgr = context->get_entity_manager();

    // Register and create tables
    entity_mgr.register_entity<User>();
    entity_mgr.create_tables(db);

    // Query
    auto admins = entity_mgr.query<User>(db)
        .where("is_active = true")
        .order_by("username")
        .limit(50)
        .execute();
}
```

### Blog Post with Foreign Key

```cpp
class Post : public entity_base {
    ENTITY_TABLE("posts")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(int64_t, user_id, foreign_key("users", "id") | not_null())
    ENTITY_FIELD(std::string, title, not_null() | index("idx_title"))
    ENTITY_FIELD(std::string, content, not_null())
    ENTITY_FIELD(std::chrono::system_clock::time_point, published_at, default_now())

    ENTITY_METADATA()
};

void Post::initialize_metadata() {
    metadata_.add_field(id_field());
    metadata_.add_field(user_id_field());
    metadata_.add_field(title_field());
    metadata_.add_field(content_field());
    metadata_.add_field(published_at_field());
}
```

Generated SQL:

```sql
CREATE TABLE IF NOT EXISTS posts (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  user_id BIGINT NOT NULL,
  title VARCHAR(255) NOT NULL,
  content VARCHAR(255) NOT NULL,
  published_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (user_id) REFERENCES users(id)
)

CREATE INDEX IF NOT EXISTS idx_title ON posts(title);
```

---

## Limitations

| Limitation | Details | Workaround |
|------------|---------|------------|
| Fixed SQL type mapping | `std::string` always maps to `VARCHAR(255)` | Use raw SQL for `TEXT` or custom lengths |
| No parameterized queries | Where clauses are raw SQL strings | Manually sanitize inputs |
| No lazy loading of relations | No `ENTITY_RELATIONSHIP` macro in current implementation | Use separate queries for related entities |
| Destructive sync_schema | `sync_schema()` drops and recreates tables | Use `create_tables()` with `IF NOT EXISTS` |
| No migration diffing | Cannot detect schema differences | Manually manage ALTER TABLE statements |
| No connection-aware CRUD | `save()`, `load()`, `update()`, `remove()` do not take a database parameter in the base class | Use `query_builder` for database-aware operations |
| `AUTO_INCREMENT` syntax | Uses MySQL syntax; PostgreSQL uses `SERIAL`/`BIGSERIAL` | Adjust DDL manually per backend |
| Metadata init not thread-safe | `ENTITY_METADATA()` lazy init uses non-atomic `static bool` | Call `get_metadata()` from single thread or synchronize externally |

---

## Related Documentation

- [API Reference](API_REFERENCE.md) - Complete API documentation including ORM section
- [Features Overview](FEATURES.md) - All database_system features
- [Architecture Overview](ARCHITECTURE.md) - System architecture and design patterns
