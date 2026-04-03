---
doc_id: "DBS-API-002"
doc_title: "Database System API Reference"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "API"
---

# Database System API Reference

> **SSOT**: This document is the single source of truth for **Database System API Reference**.

> **Language:** **English** | [한국어](API_REFERENCE.kr.md)

Complete API reference for the Database System C++20 library with multi-backend support, connection pooling, and query builders.

## Table of Contents

- [Core Classes](#core-classes)
- [Database Manager](#database-manager)
- [Connection Pooling](#connection-pooling)
- [Query Builders](#query-builders)
- [ORM Framework](#orm-framework)
- [Performance Monitoring](#performance-monitoring)
- [Security Framework](#security-framework)
- [Async Operations](#async-operations)
- [Database Types](#database-types)
- [C++20 Concepts](#c20-concepts)
- [Error Handling](#error-handling)
- [Proxy Mode](#proxy-mode)
- [Unified Database System](#unified-database-system)
- [Examples](#examples)

## Core Classes

### database_base

Abstract base class for all database implementations.

```cpp
class database_base
{
public:
    virtual ~database_base() = default;

    // Database identification
    virtual database_types database_type() = 0;

    // Connection management
    virtual bool connect(const std::string& connection_string) = 0;
    virtual bool disconnect() = 0;

    // Query operations
    virtual bool create_query(const std::string& query_string) = 0;
    virtual unsigned int insert_query(const std::string& query_string) = 0;
    virtual unsigned int update_query(const std::string& query_string) = 0;
    virtual unsigned int delete_query(const std::string& query_string) = 0;
    virtual database_result select_query(const std::string& query_string) = 0;
};
```

### database_manager

Class for managing database connections and operations. Uses dependency injection
pattern with `database_context` for testability.

```cpp
class database_manager
{
public:
    // Constructor with dependency injection
    explicit database_manager(std::shared_ptr<database_context> context);

    // Database configuration
    bool set_mode(const database_types& database_type);
    database_types database_type();

    // Connection management (DEPRECATED - use Result-based API)
    [[deprecated]] bool connect(const std::string& connection_string);
    [[deprecated]] bool disconnect();

    // Query operations (DEPRECATED - use Result-based API)
    [[deprecated]] bool create_query(const std::string& query_string);
    [[deprecated]] unsigned int insert_query(const std::string& query_string);
    [[deprecated]] unsigned int update_query(const std::string& query_string);
    [[deprecated]] unsigned int delete_query(const std::string& query_string);
    [[deprecated]] database_result select_query(const std::string& query_string);

    // Result-based API (RECOMMENDED)
    VoidResult connect_result(const std::string& connection_string);
    VoidResult disconnect_result();
    VoidResult create_query_result(const std::string& query_string);
    Result<uint64_t> insert_query_result(const std::string& query_string);
    Result<uint64_t> update_query_result(const std::string& query_string);
    Result<uint64_t> delete_query_result(const std::string& query_string);
    Result<database_result> select_query_result(const std::string& query_string);

    // Phase 3: Advanced Features
    bool create_connection_pool(database_types db_type, const connection_pool_config& config);
    std::shared_ptr<connection_pool_base> get_connection_pool(database_types db_type);
    std::map<database_types, connection_stats> get_pool_stats() const;
    query_builder create_query_builder();
    query_builder create_query_builder(database_types db_type);

    // Phase 4: Enterprise Features
    bool execute_query(const std::string& query_string);
    std::shared_ptr<orm::entity_manager> get_entity_manager();
    std::shared_ptr<monitoring::performance_monitor> get_performance_monitor();
    std::shared_ptr<security::access_control> get_access_control();
    std::shared_ptr<async::async_database> get_async_database();
};
```

## Database Manager

### Basic Usage

```cpp
#include <database/database_manager.h>
#include <database/core/database_context.h>
using namespace database;

// Create manager with dependency injection
auto context = std::make_shared<database_context>();
auto db = std::make_shared<database_manager>(context);

// Set database type
db->set_mode(database_types::postgres);

// Connect using Result-based API
std::string conn_str = "host=localhost port=5432 dbname=test user=admin password=secret";
auto connect_result = db->connect_result(conn_str);
if (connect_result.is_err()) {
    std::cerr << "Connection failed: " << connect_result.error().message << std::endl;
    return;
}

// Execute queries using Result-based API
auto create_result = db->create_query_result("CREATE TABLE users (id SERIAL PRIMARY KEY, name VARCHAR(100))");
if (create_result.is_err()) {
    std::cerr << "Create table failed: " << create_result.error().message << std::endl;
}

auto insert_result = db->insert_query_result("INSERT INTO users (name) VALUES ('John')");
if (insert_result.is_ok()) {
    std::cout << "Inserted " << insert_result.value() << " rows" << std::endl;
}

auto select_result = db->select_query_result("SELECT * FROM users");
if (select_result.is_ok()) {
    for (const auto& row : select_result.value()) {
        // Process row
    }
}

// Disconnect
db->disconnect_result();
```

### Supported Methods

#### Configuration Methods
| Method | Description | Returns |
|--------|-------------|---------|
| `set_mode(database_types)` | Set database backend type | `bool` success |
| `database_type()` | Get current database type | `database_types` |

#### Result-based API (Recommended)
| Method | Description | Returns |
|--------|-------------|---------|
| `connect_result(connection_string)` | Connect to database | `VoidResult` |
| `disconnect_result()` | Disconnect from database | `VoidResult` |
| `create_query_result(query)` | Execute DDL query | `VoidResult` |
| `insert_query_result(query)` | Execute INSERT query | `Result<uint64_t>` |
| `update_query_result(query)` | Execute UPDATE query | `Result<uint64_t>` |
| `delete_query_result(query)` | Execute DELETE query | `Result<uint64_t>` |
| `select_query_result(query)` | Execute SELECT query | `Result<database_result>` |

#### Legacy API (Deprecated)
| Method | Description | Returns |
|--------|-------------|---------|
| `connect(connection_string)` | ~~Connect to database~~ | `bool` |
| `disconnect()` | ~~Disconnect from database~~ | `bool` |
| `create_query(query)` | ~~Execute DDL query~~ | `bool` |
| `insert_query(query)` | ~~Execute INSERT query~~ | `unsigned int` |
| `update_query(query)` | ~~Execute UPDATE query~~ | `unsigned int` |
| `delete_query(query)` | ~~Execute DELETE query~~ | `unsigned int` |
| `select_query(query)` | ~~Execute SELECT query~~ | `database_result` |

> **Note**: Legacy methods are deprecated as of v1.x. Use Result-based API for proper error handling.
> See [Migration Guide](migration/legacy_api.md) for migration instructions.

## Connection Pooling

### connection_pool_config

Configuration structure for connection pools.

```cpp
struct connection_pool_config
{
    size_t min_connections = 2;                              // Minimum connections to maintain
    size_t max_connections = 20;                             // Maximum connections allowed
    std::chrono::milliseconds acquire_timeout{5000};         // Timeout for acquiring connections
    std::chrono::milliseconds idle_timeout{30000};           // Timeout for idle connections
    std::chrono::milliseconds health_check_interval{60000};   // Health check interval
    bool enable_health_checks = true;                        // Enable periodic health checks
    std::string connection_string;                           // Database connection string
};
```

### connection_stats

Statistics structure for monitoring connection pools.

```cpp
struct connection_stats
{
    size_t total_connections = 0;                             // Total connections created
    size_t active_connections = 0;                            // Currently active connections
    size_t available_connections = 0;                         // Available connections in pool
    size_t failed_acquisitions = 0;                           // Number of failed acquisitions
    size_t successful_acquisitions = 0;                       // Number of successful acquisitions
    std::chrono::steady_clock::time_point last_health_check;  // Last health check time
};
```

### connection_pool_base

Abstract base class for connection pools.

```cpp
class connection_pool_base
{
public:
    virtual ~connection_pool_base() = default;

    virtual std::shared_ptr<connection_wrapper> acquire_connection() = 0;
    virtual void release_connection(std::shared_ptr<connection_wrapper> connection) = 0;
    virtual size_t active_connections() const = 0;
    virtual size_t available_connections() const = 0;
    virtual connection_stats get_stats() const = 0;
    virtual void shutdown() = 0;
};
```

### Usage Example

```cpp
#include <database/database_manager.h>
#include <database/connection_pool.h>

// Configure connection pool
connection_pool_config config;
config.min_connections = 5;
config.max_connections = 20;
config.acquire_timeout = std::chrono::seconds(5);
config.connection_string = "host=localhost port=5432 dbname=test user=admin password=secret";

// Create pool
database_manager& db = database_manager::handle();
if (!db.create_connection_pool(database_types::postgres, config)) {
    // Handle error
}

// Use pool
auto pool = db.get_connection_pool(database_types::postgres);
auto connection = pool->acquire_connection();

if (connection) {
    // Use connection
    auto result = connection->select_query("SELECT * FROM users");

    // Connection automatically returned to pool when destroyed
}

// Monitor statistics
auto stats = db.get_pool_stats();
for (const auto& [db_type, stat] : stats) {
    std::cout << "Active: " << stat.active_connections
              << " Available: " << stat.available_connections << std::endl;
}
```

## Query Builders

### query_builder

Universal query builder that adapts to different database types using the Strategy pattern.
This is the **recommended** class for all query building operations.

> **Memory Efficiency**: Uses a single dialect allocation per builder (~66% reduction vs previous implementation)

```cpp
class query_builder
{
public:
    explicit query_builder(database_types db_type = database_types::none);
    ~query_builder() = default;

    // Move-only (dialect ownership)
    query_builder(query_builder&&) noexcept = default;
    query_builder& operator=(query_builder&&) noexcept = default;

    // Database type selection
    query_builder& for_database(database_types db_type);

    // SQL-style interface (PostgreSQL, SQLite)
    query_builder& select(const std::vector<std::string>& columns);
    query_builder& from(const std::string& table);
    query_builder& where(const std::string& field, const std::string& op, const database_value& value);
    query_builder& where(const query_condition& condition);
    query_builder& join(const std::string& table, const std::string& condition, join_type type = join_type::inner);
    query_builder& order_by(const std::string& column, sort_order order = sort_order::asc);
    query_builder& group_by(const std::vector<std::string>& columns);
    query_builder& group_by(const std::string& column);
    query_builder& having(const std::string& condition);
    query_builder& limit(size_t count);
    query_builder& offset(size_t count);

    // INSERT operations
    query_builder& insert_into(const std::string& table);
    query_builder& values(const std::map<std::string, database_value>& data);
    query_builder& values(const std::vector<std::map<std::string, database_value>>& rows);

    // UPDATE operations
    query_builder& update(const std::string& table);
    query_builder& set(const std::string& field, const database_value& value);
    query_builder& set(const std::map<std::string, database_value>& data);

    // DELETE operations
    query_builder& delete_from(const std::string& table);

    // NoSQL-style interface
    query_builder& collection(const std::string& name); // MongoDB
    query_builder& key(const std::string& key);         // Redis

    // Legacy operations (deprecated)
    [[deprecated("Use insert_into().values() instead")]]
    query_builder& insert(const std::map<std::string, database_value>& data);
    [[deprecated("Use update(table).set(data) instead")]]
    query_builder& update(const std::map<std::string, database_value>& data);
    [[deprecated("Use delete_from() instead")]]
    query_builder& remove();

    // Build and execute
    std::string build() const;
    database_result execute(database_base* db) const;

    // Reset builder
    void reset();

    // Get current database type
    database_types get_database_type() const;
};
```

### sql_query_builder (DEPRECATED)

> **⚠️ DEPRECATED**: Use `query_builder` instead. This class will be removed in a future release.

Specialized query builder for SQL databases.

```cpp
class sql_query_builder
{
public:
    // SELECT operations
    sql_query_builder& select(const std::vector<std::string>& columns);
    sql_query_builder& select(const std::string& column);
    sql_query_builder& select_raw(const std::string& raw_select);
    sql_query_builder& from(const std::string& table);

    // WHERE conditions
    sql_query_builder& where(const std::string& field, const std::string& op, const database_value& value);
    sql_query_builder& where(const query_condition& condition);
    sql_query_builder& where_raw(const std::string& raw_where);
    sql_query_builder& or_where(const std::string& field, const std::string& op, const database_value& value);

    // JOIN operations
    sql_query_builder& join(const std::string& table, const std::string& condition, join_type type = join_type::inner);
    sql_query_builder& left_join(const std::string& table, const std::string& condition);
    sql_query_builder& right_join(const std::string& table, const std::string& condition);

    // GROUP BY and HAVING
    sql_query_builder& group_by(const std::vector<std::string>& columns);
    sql_query_builder& group_by(const std::string& column);
    sql_query_builder& having(const std::string& condition);

    // ORDER BY
    sql_query_builder& order_by(const std::string& column, sort_order order = sort_order::asc);
    sql_query_builder& order_by_raw(const std::string& raw_order);

    // LIMIT and OFFSET
    sql_query_builder& limit(size_t count);
    sql_query_builder& offset(size_t count);

    // INSERT operations
    sql_query_builder& insert_into(const std::string& table);
    sql_query_builder& values(const std::map<std::string, database_value>& data);
    sql_query_builder& values(const std::vector<std::map<std::string, database_value>>& rows);

    // UPDATE operations
    sql_query_builder& update(const std::string& table);
    sql_query_builder& set(const std::string& field, const database_value& value);
    sql_query_builder& set(const std::map<std::string, database_value>& data);

    // DELETE operations
    sql_query_builder& delete_from(const std::string& table);

    // Build final query
    std::string build() const;
    std::string build_for_database(database_types db_type) const;

    // Reset builder
    void reset();
};
```

### mongodb_query_builder (DEPRECATED)

> **⚠️ DEPRECATED**: Use `query_builder` instead. This class will be removed in a future release.

Specialized query builder for MongoDB.

```cpp
class mongodb_query_builder
{
public:
    // Collection operations
    mongodb_query_builder& collection(const std::string& name);

    // Find operations
    mongodb_query_builder& find(const std::map<std::string, database_value>& filter = {});
    mongodb_query_builder& find_one(const std::map<std::string, database_value>& filter = {});

    // Projection
    mongodb_query_builder& project(const std::vector<std::string>& fields);
    mongodb_query_builder& exclude(const std::vector<std::string>& fields);

    // Sorting
    mongodb_query_builder& sort(const std::map<std::string, int>& sort_spec);
    mongodb_query_builder& sort(const std::string& field, int direction = 1);

    // Limit and Skip
    mongodb_query_builder& limit(size_t count);
    mongodb_query_builder& skip(size_t count);

    // Insert operations
    mongodb_query_builder& insert_one(const std::map<std::string, database_value>& document);
    mongodb_query_builder& insert_many(const std::vector<std::map<std::string, database_value>>& documents);

    // Update operations
    mongodb_query_builder& update_one(const std::map<std::string, database_value>& filter,
                                     const std::map<std::string, database_value>& update);
    mongodb_query_builder& update_many(const std::map<std::string, database_value>& filter,
                                      const std::map<std::string, database_value>& update);

    // Delete operations
    mongodb_query_builder& delete_one(const std::map<std::string, database_value>& filter);
    mongodb_query_builder& delete_many(const std::map<std::string, database_value>& filter);

    // Aggregation pipeline
    mongodb_query_builder& match(const std::map<std::string, database_value>& conditions);
    mongodb_query_builder& group(const std::map<std::string, database_value>& group_spec);
    mongodb_query_builder& unwind(const std::string& field);

    // Build final query
    std::string build() const;
    std::string build_json() const;

    // Reset builder
    void reset();
};
```

### redis_query_builder (DEPRECATED)

> **⚠️ DEPRECATED**: Use `query_builder` instead. This class will be removed in a future release.

Specialized query builder for Redis.

```cpp
class redis_query_builder
{
public:
    // String operations
    redis_query_builder& set(const std::string& key, const std::string& value);
    redis_query_builder& get(const std::string& key);
    redis_query_builder& del(const std::string& key);
    redis_query_builder& exists(const std::string& key);

    // Hash operations
    redis_query_builder& hset(const std::string& key, const std::string& field, const std::string& value);
    redis_query_builder& hget(const std::string& key, const std::string& field);
    redis_query_builder& hdel(const std::string& key, const std::string& field);
    redis_query_builder& hgetall(const std::string& key);

    // List operations
    redis_query_builder& lpush(const std::string& key, const std::string& value);
    redis_query_builder& rpush(const std::string& key, const std::string& value);
    redis_query_builder& lpop(const std::string& key);
    redis_query_builder& rpop(const std::string& key);
    redis_query_builder& lrange(const std::string& key, int start, int stop);

    // Set operations
    redis_query_builder& sadd(const std::string& key, const std::string& member);
    redis_query_builder& srem(const std::string& key, const std::string& member);
    redis_query_builder& sismember(const std::string& key, const std::string& member);
    redis_query_builder& smembers(const std::string& key);

    // Expiration
    redis_query_builder& expire(const std::string& key, int seconds);
    redis_query_builder& ttl(const std::string& key);

    // Build command
    std::string build() const;
    std::vector<std::string> build_args() const;

    // Reset builder
    void reset();
};
```

### Query Builder Examples

```cpp
// SQL Query Builder
auto sql_query = db.create_query_builder(database_types::postgres)
    .select({"name", "email", "created_at"})
    .from("users")
    .where("age", ">", database_value{int64_t(18)})
    .where("status", "=", database_value{std::string("active")})
    .order_by("created_at", sort_order::desc)
    .limit(10);

std::string query = sql_query.build();
// Output: SELECT "name", "email", "created_at" FROM "users" WHERE age > 18 AND status = 'active' ORDER BY created_at DESC LIMIT 10

// MongoDB Query Builder
auto mongo_query = db.create_query_builder(database_types::mongodb)
    .collection("users")
    .find({{"status", database_value{std::string("active")}}})
    .sort("created_at", -1)
    .limit(10);

std::string mongo_cmd = mongo_query.build();
// Output: db.users.find({ "status": "active" }).sort({"created_at": -1}).limit(10)

// Redis Query Builder
auto redis_query = db.create_query_builder(database_types::redis)
    .hget("user:123", "email");

std::string redis_cmd = redis_query.build();
// Output: HGET user:123 email
```

## Database Types

### database_types

Enumeration of supported database types.

```cpp
enum class database_types : uint8_t
{
    none = 0,           // No database backend
    postgres = 1,       // PostgreSQL backend
    sqlite = 3,         // SQLite backend
    oracle = 4,         // Oracle backend (future)
    mongodb = 5,        // MongoDB backend
    redis = 6           // Redis backend
};
```

### database_value

Variant type for database values.

```cpp
using database_value = std::variant<std::string, int64_t, double, bool, std::monostate>;
```

### database_result

Type definitions for database results.

```cpp
using database_row = std::map<std::string, database_value>;
using database_result = std::vector<database_row>;
```

### Working with database_value

```cpp
// Creating values
database_value str_val{std::string("hello")};
database_value int_val{int64_t(42)};
database_value double_val{3.14};
database_value bool_val{true};
database_value null_val{std::monostate{}};

// Visiting values
std::visit([](const auto& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::string>) {
        std::cout << "String: " << value << std::endl;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        std::cout << "Integer: " << value << std::endl;
    } else if constexpr (std::is_same_v<T, double>) {
        std::cout << "Double: " << value << std::endl;
    } else if constexpr (std::is_same_v<T, bool>) {
        std::cout << "Boolean: " << (value ? "true" : "false") << std::endl;
    } else if constexpr (std::is_same_v<T, std::monostate>) {
        std::cout << "NULL" << std::endl;
    }
}, str_val);
```

## C++20 Concepts

The database_system provides C++20 concepts for compile-time type validation, offering clearer error messages, self-documenting code, and better IDE support.

### Overview

**Header**: `#include <database/core/concepts.h>`
**Namespace**: `database::concepts`

### Benefits of Using Concepts

- **Clearer error messages**: Template errors are displayed as concept violations instead of hundreds of lines of SFINAE failures
- **Self-documenting code**: Concepts express type requirements explicitly
- **Better IDE support**: More accurate auto-completion and type hints
- **Code simplification**: Eliminates `std::enable_if` boilerplate

### Callable Concepts

| Concept | Description | Signature |
|---------|-------------|-----------|
| `Invocable<F, Args...>` | Callable with given arguments | `F(Args...)` |
| `VoidCallable<F, Args...>` | Callable returning void | `void F(Args...)` |
| `ReturnsResult<F, R, Args...>` | Callable returning type R | `R F(Args...)` |
| `Predicate<F, Args...>` | Callable returning bool | `bool F(Args...)` |
| `NoexceptCallable<F, Args...>` | Callable marked noexcept | `noexcept F(Args...)` |
| `DelayedCallable<F>` | Callable for delayed execution | `void F()` + move constructible |
| `AsyncCallable<F, R>` | Callable for async execution | `R F()` |

### Database-Specific Concepts

| Concept | Description | Signature |
|---------|-------------|-----------|
| `QueryCallback<F, ResultType>` | Handles query results | `void F(ResultType)` |
| `ErrorHandler<F>` | Handles database errors | `void F(const std::exception&)` |
| `ConnectionFactory<F>` | Creates database connections | `std::unique_ptr<database_base> F()` |
| `BackendFactory<F>` | Creates database backends | `std::unique_ptr<database_backend> F()` |

### Stream Concepts

| Concept | Description | Signature |
|---------|-------------|-----------|
| `StreamEventHandler<F, EventType>` | Handles stream events | `void F(const EventType&)` |
| `StreamEventFilter<F, EventType>` | Filters stream events | `bool F(const EventType&)` |

### Transaction Concepts

| Concept | Description | Usage |
|---------|-------------|-------|
| `TransactionAction<F>` | Transaction action | Saga pattern forward actions |
| `CompensationAction<F>` | Compensation (rollback) action | Saga pattern rollback actions |

### Task Execution Concepts

| Concept | Description | Usage |
|---------|-------------|-------|
| `SubmittableTask<F, Args...>` | Callable for async executor submission | `async_executor.submit()` |
| `VoidTask<F, Args...>` | Fire-and-forget callable | Background tasks |

### Pool Concepts

| Concept | Description | Requirements |
|---------|-------------|--------------|
| `PooledResource<T>` | Resource managed by a pool | Class type, default constructible |
| `ConnectionWrapper<T>` | Wraps a database connection | `get()` returns `database_base*`, `is_valid()` returns bool |

### Usage Examples

#### Type-Safe Async Task Submission

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept-constrained function for type-safe task submission
template<SubmittableTask<int> F>
auto submit_computation(async_executor& executor, F&& func) {
    return executor.submit(std::forward<F>(func));
}

// Usage
auto future = submit_computation(executor, []() { return 42; });
```

#### Query Callback Registration

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Register a type-safe query callback
template<QueryCallback<database_result> F>
void on_query_complete(F&& callback) {
    query_callbacks_.push_back(std::forward<F>(callback));
}

// Usage
on_query_complete([](const database_result& result) {
    std::cout << "Query returned " << result.size() << " rows" << std::endl;
});
```

#### Error Handler with Concept Constraint

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Set an error handler with compile-time type validation
template<ErrorHandler F>
void set_error_handler(F&& handler) {
    error_handler_ = std::forward<F>(handler);
}

// Usage
set_error_handler([](const std::exception& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
});
```

#### Stream Event Processing

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Register event handler with concept constraint
template<StreamEventHandler<stream_event> F>
void register_handler(const std::string& channel, F&& handler) {
    handlers_[channel] = std::forward<F>(handler);
}

// Register event filter with concept constraint
template<StreamEventFilter<stream_event> F>
void add_filter(const std::string& channel, F&& filter) {
    filters_[channel] = std::forward<F>(filter);
}

// Usage
register_handler("user_updates", [](const stream_event& event) {
    process_user_update(event);
});

add_filter("user_updates", [](const stream_event& event) {
    return event.type == "INSERT" || event.type == "UPDATE";
});
```

#### Saga Pattern with Transaction Concepts

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
saga_builder builder;
builder.add_step(
    []() { /* Create order */ },
    []() { /* Cancel order */ }
);
builder.add_step(
    []() { /* Reserve inventory */ },
    []() { /* Release inventory */ }
);
```

### API Methods with Concept Constraints

The following methods now have C++20 concept constraints:

| Class | Method | Concept Constraint |
|-------|--------|-------------------|
| `async_executor` | `submit()` | `requires concepts::SubmittableTask<F, Args...>` |
| `async_executor_v2` | `submit()` | `requires concepts::SubmittableTask<F, Args...>` |
| `thread_adapter` | `submit()` | `requires concepts::SubmittableTask<F, Args...>` |
| `async_result<T>` | `then()` | `concepts::VoidCallable<T>` |
| `async_result<T>` | `on_error()` | `concepts::ErrorHandler` |
| `stream_processor` | `register_event_handler()` | `concepts::StreamEventHandler<stream_event>` |
| `stream_processor` | `register_global_handler()` | `concepts::StreamEventHandler<stream_event>` |
| `stream_processor` | `add_event_filter()` | `concepts::StreamEventFilter<stream_event>` |
| `saga_builder` | `add_step()` | `concepts::TransactionAction`, `concepts::CompensationAction` |

**Note:** Legacy `std::function` overloads are maintained for backward compatibility.

## Error Handling

### Exception Safety

All database operations are exception-safe with RAII resource management.

```cpp
try {
    database_manager& db = database_manager::handle();

    if (!db.set_mode(database_types::postgres)) {
        throw std::runtime_error("Failed to set database mode");
    }

    if (!db.connect(connection_string)) {
        throw std::runtime_error("Failed to connect to database");
    }

    auto result = db.select_query("SELECT * FROM users");
    // Process result

} catch (const std::exception& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
}
```

### Mock Implementations

When database libraries are not available, the system provides mock implementations that return empty results but don't throw exceptions.

```cpp
// Even without PostgreSQL libraries, this won't crash
database_manager& db = database_manager::handle();
db.set_mode(database_types::postgres);

if (!db.connect("mock://connection")) {
    // This will fail gracefully with mock implementation
    std::cout << "Mock connection - no actual database required" << std::endl;
}

auto result = db.select_query("SELECT * FROM users");
// Returns empty result with mock implementation
```

## Examples

### Complete Usage Example

```cpp
#include <database/database_manager.h>
#include <database/connection_pool.h>
#include <database/query_builder.h>
#include <iostream>

int main() {
    try {
        // Initialize database manager
        database_manager& db = database_manager::handle();

        // Configure PostgreSQL
        if (!db.set_mode(database_types::postgres)) {
            std::cerr << "Failed to set database mode" << std::endl;
            return 1;
        }

        // Setup connection pool
        connection_pool_config config;
        config.min_connections = 2;
        config.max_connections = 10;
        config.connection_string = "host=localhost port=5432 dbname=test user=admin password=secret";

        if (!db.create_connection_pool(database_types::postgres, config)) {
            std::cerr << "Failed to create connection pool" << std::endl;
            return 1;
        }

        // Create table using query builder
        auto create_table = db.create_query_builder()
            .create_raw("CREATE TABLE IF NOT EXISTS users ("
                       "id SERIAL PRIMARY KEY, "
                       "name VARCHAR(100) NOT NULL, "
                       "email VARCHAR(100) UNIQUE, "
                       "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");

        // Insert data
        auto insert_query = db.create_query_builder()
            .insert_into("users")
            .values({
                {"name", database_value{std::string("John Doe")}},
                {"email", database_value{std::string("john@example.com")}}
            });

        // Select data with conditions
        auto select_query = db.create_query_builder()
            .select({"id", "name", "email", "created_at"})
            .from("users")
            .where("name", "LIKE", database_value{std::string("%John%")})
            .order_by("created_at", sort_order::desc)
            .limit(10);

        // Execute queries
        auto pool = db.get_connection_pool(database_types::postgres);
        auto connection = pool->acquire_connection();

        if (connection) {
            // Execute through connection
            auto result = select_query.execute(connection.get());

            // Process results
            for (const auto& row : result) {
                for (const auto& [column, value] : row) {
                    std::cout << column << ": ";
                    std::visit([](const auto& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, std::monostate>) {
                            std::cout << "NULL";
                        } else {
                            std::cout << v;
                        }
                    }, value);
                    std::cout << " ";
                }
                std::cout << std::endl;
            }
        }

        // Monitor pool statistics
        auto stats = db.get_pool_stats();
        for (const auto& [db_type, stat] : stats) {
            std::cout << "Pool Stats - Active: " << stat.active_connections
                      << " Available: " << stat.available_connections << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Multi-Database Example

```cpp
#include <database/database_manager.h>
#include <database/query_builder.h>

int main() {
    database_manager& db = database_manager::handle();

    // PostgreSQL operations
    auto pg_query = db.create_query_builder(database_types::postgres)
        .select({"id", "name"})
        .from("users")
        .where("status", "=", database_value{std::string("active")});

    std::cout << "PostgreSQL: " << pg_query.build() << std::endl;

    // MongoDB operations
    auto mongo_query = db.create_query_builder(database_types::mongodb)
        .collection("users")
        .find({{"status", database_value{std::string("active")}}})
        .project({"_id", "name"});

    std::cout << "MongoDB: " << mongo_query.build() << std::endl;

    // Redis operations
    auto redis_query = db.create_query_builder(database_types::redis)
        .hgetall("user:123");

    std::cout << "Redis: " << redis_query.build() << std::endl;

    return 0;
}
```

---

## Phase 4: Enterprise APIs

### ORM Framework

> Full documentation: [ORM Framework Guide](ORM_GUIDE.md)

```cpp
#include <database/orm/entity.h>
using namespace database::orm;

// Entity definition with declarative macros
class User : public entity_base {
    ENTITY_TABLE("users")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique() | index("idx_username"))
    ENTITY_FIELD(std::string, email, not_null() | unique())
    ENTITY_METADATA()
};

// Register entity and create tables
auto& entity_mgr = context->get_entity_manager();
entity_mgr.register_entity<User>();
entity_mgr.create_tables(db);

// Type-safe query builder
auto users = entity_mgr.query<User>(db)
    .where("is_active = true")
    .order_by("username")
    .limit(100)
    .execute();
```

**Key classes**: `entity_base`, `entity_manager`, `query_builder<T>`, `field_metadata`, `entity_metadata`

**Macros**: `ENTITY_TABLE(table)`, `ENTITY_FIELD(type, name, constraints...)`, `ENTITY_METADATA()`

### Performance Monitoring

```cpp
#include <database/monitoring/performance_monitor.h>

// Performance monitoring
auto& monitor = performance_monitor::instance();
monitor.set_alert_thresholds(0.05, std::chrono::milliseconds(1000));
auto summary = monitor.get_performance_summary();
```

### Security Framework

```cpp
#include <database/security/secure_connection.h>

// Access control
auto& access = access_control::instance();
access.create_role(admin_role);
bool allowed = access.check_permission("user123", "users", "SELECT");

// Audit logging
AUDIT_LOG_ACCESS("user123", "session456", "SELECT", "users", "query_hash", true, "");
```

### Async Operations

> Full documentation: [Async Operations Guide](guides/ASYNC_OPERATIONS.md)

**Header:** `database/async/async_operations.h`
**Namespace:** `database::async`

```cpp
#include <database/async/async_operations.h>
using namespace database::async;

// async_executor — thread pool with SubmittableTask concept
async_executor executor(4);
auto future = executor.submit([]() { return compute(); });

// async_database — async wrapper for database_backend
async_database db(backend, executor_ptr);
auto result = db.execute_async("INSERT INTO users (name) VALUES ('Alice')");
auto rows = db.select_async("SELECT * FROM users");

// Batch operations
auto batch = db.execute_batch_async({"INSERT ...", "INSERT ..."});

// Callback support
result.then([](bool ok) { /* success */ });
result.on_error([](const std::exception& e) { /* error */ });

// C++20 Coroutine support (requires HAS_COROUTINES)
database_awaitable<bool> async_op() {
    co_return co_await db.execute_coro("SELECT * FROM users");
}

// Stream processing
stream_processor processor(backend);
processor.start_stream(stream_processor::stream_type::postgresql_notify, "events");
processor.register_event_handler("events", [](const auto& event) { /* handle */ });

// Saga pattern for distributed transactions
auto saga = txn_coord.create_saga();
saga.add_step(forward_action, compensation_action);
saga.execute();
```

**Key classes:** `async_executor`, `async_result<T>`, `async_database`, `database_awaitable<T>`, `stream_processor`, `transaction_coordinator`, `saga_builder`

---

## Proxy Mode

**Status**: Development Preview (Stub Implementation)
**Headers**: `database/proxy/proxy_config.h`, `database/proxy/proxy_connector.h`
**Namespace**: `database::proxy`

> Full documentation: [Proxy Layer Guide](guides/PROXY_LAYER.md)

### proxy_connection_config

Configuration structure for connecting to `database_server` middleware.

```cpp
#include <database/proxy/proxy_config.h>

database::proxy::proxy_connection_config config;
config.server_host = "db-gateway.internal";
config.server_port = 9432;
config.auth_token = "client-token";
config.connection_timeout = std::chrono::milliseconds{5000};
config.query_timeout = std::chrono::milliseconds{30000};
config.use_tls = true;
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `server_host` | `std::string` | `"localhost"` | database_server hostname |
| `server_port` | `uint16_t` | `9432` | database_server port |
| `auth_token` | `std::string` | `""` | Authentication token |
| `connection_timeout` | `milliseconds` | `5000` | Connection timeout |
| `query_timeout` | `milliseconds` | `30000` | Query execution timeout |
| `retry_count` | `uint8_t` | `3` | Retry attempts |
| `retry_delay` | `milliseconds` | `1000` | Delay between retries |
| `use_tls` | `bool` | `true` | Enable TLS encryption |
| `ca_cert_path` | `std::string` | `""` | CA certificate path |
| `client_cert_path` | `std::string` | `""` | Client certificate (mTLS) |
| `client_key_path` | `std::string` | `""` | Client private key (mTLS) |

### proxy_connector

Implements `database_backend` interface for proxy mode. All queries are sent to a `database_server` middleware.

```cpp
#include <database/proxy/proxy_connector.h>

auto connector = std::make_unique<database::proxy::proxy_connector>(
    database::database_types::postgres, config);

// Initialize (connects to database_server)
database::core::connection_config conn_config;
auto result = connector->initialize(conn_config);

// Query operations (same interface as direct backends)
auto rows = connector->select_query("SELECT * FROM users");
connector->shutdown();
```

### database_manager Proxy API

| Method | Description | Returns |
|--------|-------------|---------|
| `set_mode_proxy(db_type, proxy_config)` | Switch to proxy mode | `bool` success |
| `current_connection_mode()` | Get current mode (direct/proxy) | `connection_mode` |

```cpp
auto db_mgr = std::make_shared<database::database_manager>(context);

// Switch to proxy mode
db_mgr->set_mode_proxy(database::database_types::postgres, proxy_config);

// Check mode
if (db_mgr->current_connection_mode() == database::connection_mode::proxy) {
    // Running in proxy mode
}

// Switch back to direct mode
db_mgr->set_mode(database::database_types::postgres);
```

---

## Unified Database System

> Full documentation: [Unified System Guide](guides/UNIFIED_SYSTEM.md)

**Headers**: `database/integrated/unified_database_system.h`, `database/integrated/core/configuration.h`
**Namespace**: `database::integrated`

The unified database system provides a single entry point for database operations with integrated logging, monitoring, and async support.

### Quick Start

```cpp
#include <database/integrated/unified_database_system.h>
using namespace database::integrated;

// Zero-config usage
unified_database_system db;
db.connect();

// Or with full configuration
auto config = unified_db_config{}
    .set_backend(backend_type::postgres, "host=localhost dbname=mydb")
    .set_pool_size(5, 20)
    .set_log_level(db_log_level::info)
    .enable_monitoring(true)
    .set_thread_count(4);

database_coordinator coordinator(config);
coordinator.initialize();
```

### Key Components

| Component | Header | Description |
|-----------|--------|-------------|
| `unified_db_config` | `core/configuration.h` | Fluent builder for all configuration |
| `database_coordinator` | `core/database_coordinator.h` | Adapter lifecycle management |
| `logger_adapter` | `adapters/logger_adapter.h` | Unified logging with backend selection |
| `monitoring_adapter` | `adapters/monitoring_adapter.h` | Metrics collection and Prometheus export |
| `thread_adapter` | `adapters/thread_adapter.h` | Async task execution with `SubmittableTask` concept |
| `connection_string_builder` | `connection_string_builder.h` | Type-safe connection string construction |
| `protocol_serializer` | `protocol/database_protocol.h` | Binary protocol for client-server communication |

### Configuration Enums

| Enum | Values |
|------|--------|
| `backend_type` | `postgres`, `sqlite`, `mongodb`, `redis` |
| `db_log_level` | `trace`, `debug`, `info`, `warning`, `error`, `critical`, `fatal` |
| `thread_pool_type` | `standard`, `typed` |
| `ssl_mode` | `disable`, `allow`, `prefer`, `require`, `verify_ca`, `verify_full` |

---

## System Requirements

- **C++ Standard**: C++20
- **Supported Compilers**: GCC 10+, Clang 11+, MSVC 2019+
- **Supported Platforms**: Windows, macOS, Linux

For the latest API updates and changes, see the [CHANGELOG](../CHANGELOG.md).
---

*Last Updated: 2025-12-09*
