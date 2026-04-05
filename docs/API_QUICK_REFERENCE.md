# API Quick Reference -- database_system

Cheat-sheet for the most common `database_system` APIs.
For full details see [API_REFERENCE.md](API_REFERENCE.md) and the Doxygen-generated docs.

---

## Header and Namespace

```cpp
// Unified entry point (recommended)
#include <kcenon/database/integrated/unified_database_system.h>

// Individual components
#include <kcenon/database/query_builder.h>
#include <kcenon/database/core/backend_registry.h>

using namespace database::integrated;
```

---

## Connection Creation

### Zero-Config (simplest)

```cpp
unified_database_system db;
auto result = db.connect("postgresql://localhost/mydb");
// or
auto result = db.connect(backend_type::postgresql,
                         "host=localhost dbname=mydb user=admin");
```

### Builder Pattern

```cpp
auto db = unified_database_system::create_builder()
    .set_backend(backend_type::postgresql)
    .set_connection_string("host=localhost dbname=mydb")
    .set_pool_size(/*min=*/2, /*max=*/50)
    .enable_logging(db_log_level::info, "./logs")
    .enable_monitoring(true)
    .enable_async(/*worker_threads=*/4)
    .set_slow_query_threshold(std::chrono::milliseconds(500))
    .build();
```

### Disconnect

```cpp
db.disconnect();
bool connected = db.is_connected();
```

---

## Query Builder

```cpp
#include <kcenon/database/query_builder.h>

database::query_builder qb(database::database_types::postgres);

// SELECT
auto sql = qb.select({"id", "name", "email"})
    .from("users")
    .where("active", "=", true)
    .join("orders", "users.id = orders.user_id", database::join_type::left)
    .order_by("name", database::sort_order::asc)
    .group_by("department")
    .having("COUNT(*) > 5")
    .limit(10)
    .offset(20)
    .build();

// INSERT
auto sql = qb.insert_into("users")
    .values({{"name", "Alice"}, {"age", 30}})
    .build();

// UPDATE
auto sql = qb.update("users")
    .set("name", "Bob")
    .set({{"age", 31}, {"active", true}})
    .where("id", "=", 1)
    .build();

// DELETE
auto sql = qb.delete_from("users")
    .where("active", "=", false)
    .build();

// Execute directly
auto result = qb.select({"*"}).from("users").execute(backend_ptr);

// Reset for reuse
qb.reset();
```

### Query Conditions (composable)

```cpp
database::query_condition active("active", "=", true);
database::query_condition adult("age", ">=", 18);

auto combined = active && adult;         // AND
auto either   = active || adult;         // OR

qb.where(combined);
```

---

## ORM Macros

```cpp
#include <database/orm/entity.h>

class User : public database::orm::entity_base {
public:
    // Define table name
    ENTITY_TABLE("users")

    // Define fields with constraints
    ENTITY_FIELD(int64_t, id,
        database::orm::field_constraint::primary_key |
        database::orm::field_constraint::auto_increment)

    ENTITY_FIELD(std::string, name,
        database::orm::field_constraint::not_null)

    ENTITY_FIELD(std::string, email,
        database::orm::field_constraint::unique |
        database::orm::field_constraint::not_null)

    ENTITY_FIELD(int32_t, age,
        database::orm::field_constraint::none)

    ENTITY_FIELD(bool, active,
        database::orm::field_constraint::not_null)
};
```

### Field Constraints

| Constraint | Description |
|---|---|
| `primary_key` | Primary key column |
| `auto_increment` | Auto-incrementing value |
| `not_null` | NOT NULL constraint |
| `unique` | UNIQUE constraint |
| `index` | Create index on column |
| `foreign_key` | Foreign key reference |
| `default_now` | Default to current timestamp |

Constraints are combined with `|` (bitwise OR).

### Entity Manager

```cpp
database::orm::entity_manager mgr;

mgr.register_entity<User>();
mgr.create_tables(backend);          // CREATE TABLE IF NOT EXISTS
mgr.sync_schema(backend);            // Sync schema with DB

auto query = mgr.query<User>(backend);
auto users = query.where("active = true")
    .order_by("name")
    .limit(10)
    .execute();

auto first = query.where("id = 1").first();  // std::optional<User>
size_t n   = query.where("active = true").count();
double avg = query.avg("age");
```

### CRUD on Entities

```cpp
User user;
user.name = "Alice";
user.email = "alice@example.com";
user.save();          // INSERT
user.load();          // SELECT by primary key
user.name = "Bob";
user.update();        // UPDATE
user.remove();        // DELETE
```

---

## Transaction Management

```cpp
// Begin transaction
auto tx = db->begin_transaction();

// Execute within transaction
tx->execute("INSERT INTO users (name) VALUES ($1)", {"Alice"});
tx->execute("UPDATE accounts SET balance = balance - 100 WHERE id = $1", {1});

// Commit or rollback
if (all_ok) {
    tx->commit();
} else {
    tx->rollback();
}

// Check state
bool active = tx->is_active();
```

---

## Synchronous Query Execution

```cpp
// Generic execute
auto result = db.execute("SELECT * FROM users WHERE id = $1", {42});

// Convenience wrappers
auto rows    = db.select("SELECT * FROM users");
auto count   = db.insert("INSERT INTO users (name) VALUES ($1)", {"Alice"});
auto updated = db.update("UPDATE users SET active = $1 WHERE id = $2", {true, 1});
auto deleted = db.remove("DELETE FROM users WHERE id = $1", {99});
```

### Query Parameters (`query_param`)

```cpp
// Implicit conversions from common types
db.execute("...", {42});                  // int
db.execute("...", {"Alice"});             // string
db.execute("...", {3.14});                // double
db.execute("...", {true});                // bool
db.execute("...", {nullptr});             // SQL NULL

// Null safety for C strings
const char* ptr = get_nullable();         // may return nullptr
db.execute("...", {ptr});                 // safe -- is_null() if nullptr
```

### Query Result

```cpp
auto result = db.execute("SELECT * FROM users");
if (result.is_ok()) {
    auto& qr = result.value();
    for (auto& row : qr) {
        std::cout << row["name"] << "\n";
    }
    std::cout << "Rows: " << qr.size() << "\n";
    std::cout << "Time: " << qr.execution_time.count() << " us\n";
}
```

---

## Connection Pool

Configured via the builder:

```cpp
auto db = unified_database_system::create_builder()
    .set_pool_size(/*min=*/5, /*max=*/50)
    .build();
```

Pool metrics are available through `get_metrics()`:

```cpp
auto metrics = db->get_metrics();
std::cout << "Pool size: " << metrics.pool_size << "\n";
std::cout << "Active: " << metrics.active_connections << "\n";
std::cout << "Idle: " << metrics.idle_connections << "\n";
std::cout << "Wait queue: " << metrics.wait_queue_size << "\n";
```

---

## Backend Registry

```cpp
#include <kcenon/database/core/backend_registry.h>

auto& registry = database::core::backend_registry::instance();

// Register a custom backend
registry.register_backend("my_backend", &my_backend::create);

// Create backend instance
auto backend = registry.create("postgresql");

// List available backends
auto backends = registry.available_backends();
```

---

## Async Operations

```cpp
// Async query execution
auto future = db->execute_async("SELECT * FROM large_table");
// ... do other work ...
auto result = future.get();  // blocks until complete

// Health and metrics
auto health = db->check_health();     // health_check struct
auto metrics = db->get_metrics();     // database_metrics struct

std::cout << "Status: " << static_cast<int>(health.status) << "\n";
std::cout << "QPS: " << metrics.queries_per_second << "\n";
std::cout << "P95: " << metrics.p95_latency.count() << " us\n";
```

---

## Supported Backends

| Backend | CMake option | Connection string example |
|---|---|---|
| PostgreSQL | `USE_POSTGRESQL=ON` | `host=localhost port=5432 dbname=mydb` |
| SQLite | `USE_SQLITE=ON` | `mydb.db` or `:memory:` |
| MongoDB | `USE_MONGODB=OFF` | `mongodb://localhost:27017/mydb` |
| Redis | `USE_REDIS=OFF` | `redis://localhost:6379` |

---

## Quick Recipe

```cpp
#include <kcenon/database/integrated/unified_database_system.h>
using namespace database::integrated;

// Setup
auto db = unified_database_system::create_builder()
    .set_backend(backend_type::postgresql)
    .set_connection_string("host=localhost dbname=app")
    .set_pool_size(5, 20)
    .enable_async(4)
    .build();

// Transactional insert
auto tx = db->begin_transaction();
tx->execute("INSERT INTO users (name, email) VALUES ($1, $2)",
            {"Alice", "alice@example.com"});
tx->commit();

// Query with builder
database::query_builder qb(database::database_types::postgres);
auto sql = qb.select({"id", "name"})
    .from("users")
    .where("active", "=", true)
    .limit(10)
    .build();
auto result = db->execute(sql);
```
