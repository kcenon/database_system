# Getting Started with database_system

A step-by-step guide to integrating `database_system` into your C++ project,
from prerequisites through advanced ORM patterns.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [SQLite Connection (Quickstart)](#sqlite-connection-quickstart)
- [Query Builder](#query-builder)
- [ORM with ENTITY Macros](#orm-with-entity-macros)
- [Transactions](#transactions)
- [Connection Pool](#connection-pool)
- [Multi-Backend Switching](#multi-backend-switching)
- [Next Steps](#next-steps)

---

## Prerequisites

| Requirement | Minimum Version |
|---|---|
| C++ compiler | C++20 (GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+) |
| CMake | 3.20+ |
| common_system | latest (Tier 0 dependency) |
| vcpkg (optional) | latest, for backend-specific libraries |

Backend-specific libraries (installed via vcpkg or system package manager):

| Backend | Library | vcpkg Package |
|---|---|---|
| PostgreSQL | libpqxx >= 7.9.2 | `libpqxx` |
| SQLite | sqlite3 >= 3.45.0 | `sqlite3` |
| MongoDB | mongo-cxx-driver >= 3.8.0 | `mongo-cxx-driver` |
| Redis | hiredis >= 1.2.0 | `hiredis` |

## Installation

### Option 1: CMake FetchContent (Recommended)

```cmake
include(FetchContent)

FetchContent_Declare(
  database_system
  GIT_REPOSITORY https://github.com/kcenon/database_system.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(database_system)

target_link_libraries(your_target PRIVATE database_system::database)
```

### Option 2: Manual Build

```bash
# Clone the repository
git clone https://github.com/kcenon/database_system.git
cd database_system

# Install ecosystem dependencies
./scripts/dependency.sh

# Build with your desired backends
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DUSE_SQLITE=ON \
  -DUSE_POSTGRESQL=ON
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

Key CMake options:

| Option | Default | Description |
|---|---|---|
| `USE_POSTGRESQL` | ON | Enable PostgreSQL backend |
| `USE_SQLITE` | OFF | Enable SQLite backend |
| `USE_MONGODB` | OFF | Enable MongoDB backend (experimental) |
| `USE_REDIS` | OFF | Enable Redis backend (experimental) |
| `USE_UNIT_TEST` | ON | Build unit tests |

### Option 3: vcpkg Preset

```bash
cmake --preset=vcpkg -DUSE_SQLITE=ON
cmake --build --preset=vcpkg
```

## SQLite Connection (Quickstart)

The fastest way to try `database_system` without running a database server.

```cpp
#include "database/core/backend_registry.h"
#include "database/core/database_backend.h"

using namespace database::core;

int main() {
    // Create an SQLite backend via the registry
    auto backend = backend_registry::instance().create("sqlite");
    if (!backend) {
        std::cerr << "SQLite backend not available. "
                  << "Build with -DUSE_SQLITE=ON\n";
        return 1;
    }

    // Connect to an in-memory database (or provide a file path)
    connection_config config;
    config.database = ":memory:";  // Use "mydata.db" for persistence
    auto result = backend->initialize(config);

    if (!result.is_ok()) {
        std::cerr << "Connection failed: " << result.error().message << "\n";
        return 1;
    }

    // Create a table
    backend->execute_query(R"(
        CREATE TABLE users (
            id    INTEGER PRIMARY KEY AUTOINCREMENT,
            name  TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL
        )
    )");

    // Insert data
    backend->execute_query(
        "INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')");

    // Query data
    auto rows = backend->select_query("SELECT id, name, email FROM users");
    if (rows.is_ok()) {
        for (const auto& row : rows.value()) {
            for (const auto& [col, val] : row) {
                std::visit([&](const auto& v) {
                    std::cout << col << "=" << v << " ";
                }, val);
            }
            std::cout << "\n";
        }
    }

    backend->shutdown();
    return 0;
}
```

For PostgreSQL, replace the backend name and connection config:

```cpp
auto backend = backend_registry::instance().create("postgresql");

connection_config config;
config.host = "localhost";
config.port = 5432;
config.database = "mydb";
config.username = "myuser";
config.password = "mypass";
auto result = backend->initialize(config);
```

## Query Builder

The `immutable_query_builder` provides a thread-safe, functional-style API for
constructing SQL queries. Each method returns a **new** builder instance,
leaving the original unchanged.

```cpp
#include "database/query/immutable_query_builder.h"

using namespace database;

// Build a SELECT query
auto query = immutable_query_builder("users")
    .select({"id", "name", "email"})
    .where("active", "=", true)
    .where("age", ">=", 18)
    .order_by("name", sort_order::asc)
    .limit(10)
    .build();

// Execute the query via a backend
auto result = backend->select_query(query);
```

Composable queries -- base query can be reused:

```cpp
// Define a base query
auto active_users = immutable_query_builder("users")
    .select({"id", "name", "email"})
    .where("active", "=", true);

// Derive specific queries from the same base
auto young_users = active_users
    .where("age", "<", 30)
    .order_by("name")
    .build();

auto admins = active_users
    .where("role", "=", std::string("admin"))
    .build();
```

## ORM with ENTITY Macros

Define C++ classes that map directly to database tables using three macros:
`ENTITY_TABLE`, `ENTITY_FIELD`, and `ENTITY_METADATA`.

### Defining an Entity

```cpp
#include "database/orm/entity.h"

using namespace database::orm;

class User : public entity_base {
    ENTITY_TABLE("users")

    ENTITY_FIELD(int64_t,     id,         primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username,   not_null() | unique() | index("idx_username"))
    ENTITY_FIELD(std::string, email,      not_null() | unique())
    ENTITY_FIELD(bool,        is_active,  not_null())
    ENTITY_FIELD(std::chrono::system_clock::time_point, created_at, default_now())

    ENTITY_METADATA()

public:
    User() : is_active(true), created_at(std::chrono::system_clock::now()) {}
};

// Required: implement initialize_metadata() outside the class
void User::initialize_metadata() {
    metadata_.add_field(id_field());
    metadata_.add_field(username_field());
    metadata_.add_field(email_field());
    metadata_.add_field(is_active_field());
    metadata_.add_field(created_at_field());
}
```

### Available Field Constraints

| Constraint | Description |
|---|---|
| `primary_key()` | Marks field as the primary key |
| `auto_increment()` | Auto-incrementing value |
| `not_null()` | Field cannot be NULL |
| `unique()` | Field value must be unique |
| `index("name")` | Create a named index |
| `default_now()` | Default to current timestamp |
| `foreign_key("table", "field")` | Foreign key reference |

Constraints are combined with the `|` operator:
`primary_key() | auto_increment()`.

### Registering Entities and Generating Schema

```cpp
#include "database/core/database_context.h"

auto context = std::make_shared<database_context>();
auto entity_mgr = context->get_entity_manager();

// Register entity types
entity_mgr->register_entity<User>();

// Generate and execute CREATE TABLE statements
entity_mgr->create_tables(backend);
```

### Querying with the ORM

```cpp
// Type-safe queries through entity_manager
auto users = entity_mgr->query<User>(backend)
    .where("is_active = true")
    .order_by("username")
    .limit(10)
    .execute();

for (const auto& user : users) {
    std::cout << user.username.get() << ": " << user.email.get() << "\n";
}

// Aggregations
auto count = entity_mgr->query<User>(backend).count();
```

## Transactions

Transaction support is available through the unified database system.

```cpp
#include "integrated/unified_database_system.h"

using namespace database::integrated;

auto db = unified_database_system::create_builder()
    .enable_logging(db_log_level::info, "./logs")
    .build();

db->connect("host=localhost dbname=mydb user=myuser password=mypass");

// Begin a transaction
auto tx = db->begin_transaction();

try {
    tx->execute("INSERT INTO accounts (owner, balance) VALUES ('Alice', 1000)");
    tx->execute("INSERT INTO accounts (owner, balance) VALUES ('Bob', 500)");
    tx->execute("UPDATE accounts SET balance = balance - 100 WHERE owner = 'Alice'");
    tx->execute("UPDATE accounts SET balance = balance + 100 WHERE owner = 'Bob'");

    // All statements succeed -- commit
    tx->commit();

} catch (const std::exception& e) {
    // Any failure -- rollback all changes
    tx->rollback();
    std::cerr << "Transaction failed: " << e.what() << "\n";
}
```

Every `begin_transaction()` call creates an independent transaction context.
If the transaction object is destroyed without calling `commit()`, the
transaction is automatically rolled back (RAII).

## Connection Pool

The unified database system includes built-in connection pooling configured
through the builder pattern.

```cpp
auto db = unified_database_system::create_builder()
    .set_pool_size(5, 20)          // min 5, max 20 connections
    .enable_logging(db_log_level::info, "./logs")
    .enable_monitoring(true)       // track pool utilization
    .enable_async(8)               // 8 async worker threads
    .build();

db->connect("host=localhost dbname=mydb user=myuser password=mypass");

// Connections are automatically managed
auto result = db->execute("SELECT * FROM users");

// Check pool health
auto health = db->check_health();
std::cout << "Pool utilization: "
          << (health.connection_pool_utilization * 100) << "%\n";

// View metrics
auto metrics = db->get_metrics();
std::cout << "Active connections: " << metrics.active_connections << "\n";
std::cout << "Idle connections: "   << metrics.idle_connections   << "\n";
std::cout << "Queries/sec: "        << metrics.queries_per_second << "\n";
```

Pool parameters:

| Parameter | Builder Method | Description |
|---|---|---|
| Min/Max size | `set_pool_size(min, max)` | Connection count range |
| Async threads | `enable_async(n)` | Worker threads for `execute_async()` |
| Slow query threshold | `set_slow_query_threshold(ms)` | Log queries slower than threshold |

## Multi-Backend Switching

The `backend_registry` enables runtime backend selection without conditional
compilation. Applications can switch databases by changing a single string.

```cpp
#include "database/core/backend_registry.h"

using namespace database::core;

// List all available backends (depends on build flags)
auto backends = backend_registry::instance().available_backends();
for (const auto& name : backends) {
    std::cout << "Available backend: " << name << "\n";
}

// Select a backend at runtime (e.g., from config file or CLI argument)
std::string backend_name = "postgresql";  // or "sqlite", "mongodb", "redis"
auto backend = backend_registry::instance().create(backend_name);

if (!backend) {
    std::cerr << "Backend '" << backend_name << "' not available\n";
    return 1;
}

// All backends implement the same database_backend interface
connection_config config;
config.host = "localhost";
config.database = "mydb";
auto result = backend->initialize(config);

// Identical API regardless of backend
backend->execute_query("INSERT INTO logs (msg) VALUES ('hello')");
auto rows = backend->select_query("SELECT * FROM logs");
backend->shutdown();
```

With the unified system:

```cpp
auto db = unified_database_system::create_builder()
    .set_backend(backend_type::sqlite)   // switch here
    .set_connection_string("mydata.db")
    .build();

// Same API for all backends
auto result = db->execute("SELECT * FROM users");
```

## Next Steps

- **[API Reference](API_REFERENCE.md)** -- Complete API documentation for all
  classes and methods.
- **[API Quick Reference](API_QUICK_REFERENCE.md)** -- One-page cheat sheet
  with common patterns.
- **[ORM Guide](ORM_GUIDE.md)** -- Deep dive into entity definitions,
  relationships, and advanced queries.
- **[Architecture](ARCHITECTURE.md)** -- System design, backend abstraction,
  and adapter patterns.
- **[Backends](BACKENDS.md)** -- Backend-specific configuration and feature
  matrix.
- **[samples/](../samples/)** -- Runnable example programs covering all
  features.
- **[Benchmarks](BENCHMARKS.md)** -- Performance data for query builders,
  connection pools, and backends.
