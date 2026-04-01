# database_system Examples

Introductory C++20 examples demonstrating the core features of database_system.
Each example focuses on a single feature area and is designed to be read top-to-bottom.

## Examples

| Example | File | Database Required | Description |
|---------|------|:-----------------:|-------------|
| **Basic Connection** | `basic_connection.cpp` | Yes | Connect to PostgreSQL, create a table, insert and query data, disconnect. |
| **Query Builder** | `query_builder_usage.cpp` | No | Build SELECT, INSERT, UPDATE, DELETE queries using the fluent API. Demonstrates joins, grouping, conditions, and dialect switching. |
| **Transaction Management** | `transaction_management.cpp` | Yes | Begin, commit, and rollback transactions. Includes a RAII transaction guard pattern. |
| **Error Handling** | `error_handling.cpp` | Partial | Use `Result<T>` and `VoidResult` to handle connection failures, invalid SQL, constraint violations, and chained operations. |
| **ORM Entity** | `orm_entity_demo.cpp` | No | Define entity classes with field metadata and constraints. Generate CREATE TABLE SQL and inspect metadata programmatically. |
| **Connection Pool** | `connection_pool_demo.cpp` | Yes | Multi-threaded database usage with independent `database_manager` instances per thread. |

## Building

Examples are built when the `BUILD_DATABASE_EXAMPLES` CMake option is enabled (default: `OFF`):

```bash
cmake -B build -DBUILD_DATABASE_EXAMPLES=ON
cmake --build build
```

The example binaries are placed in `build/bin/`:

```
build/bin/example_basic_connection
build/bin/example_query_builder_usage
build/bin/example_transaction_management
build/bin/example_error_handling
build/bin/example_orm_entity_demo
build/bin/example_connection_pool_demo
```

## Running Without a Database

Two examples (`query_builder_usage` and `orm_entity_demo`) do not require a running database server. They demonstrate API usage by generating SQL strings and inspecting metadata in-memory.

## Running With PostgreSQL

For examples that require a database:

1. Start a PostgreSQL server (local or Docker).
2. Create a database:
   ```sql
   CREATE DATABASE example_db;
   CREATE USER "user" WITH PASSWORD 'password';
   GRANT ALL PRIVILEGES ON DATABASE example_db TO "user";
   ```
3. Update the connection string in the example source file if needed.
4. Run the example binary.

## Relationship to samples/

The `samples/` directory contains more comprehensive integration-level programs.
These `examples/` are intentionally simpler and focused on introducing individual
API features to new users.
