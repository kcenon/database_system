# Database System

Advanced C++20 Database System with Multi-Backend Support and High-Performance Operations

## Overview

The Database Module provides a comprehensive database abstraction layer with support for multiple database backends. It features connection pooling, prepared statements, transaction management, and thread-safe operations optimized for high-performance messaging systems.

## Features

### 🎯 Core Capabilities
- **Multi-Backend Support**: PostgreSQL, MySQL, SQLite support with unified interface
- **Connection Pooling**: Efficient connection management with automatic failover
- **Thread Safety**: Concurrent database operations with proper synchronization
- **Transaction Management**: ACID compliance with nested transaction support
- **Prepared Statements**: SQL injection prevention and performance optimization
- **Query Builder**: Type-safe query construction with compile-time validation

### 🗄️ Supported Databases

| Database | Status | Features | Performance |
|----------|--------|----------|-------------|
| PostgreSQL | ✅ Full | JSONB, Arrays, CTEs | Excellent |
| MySQL | 🔧 Planned | Full-text search, Partitioning | Very Good |
| SQLite | 🔧 Planned | WAL mode, FTS5 | Good |

### 📊 Database Types

```cpp
enum class database_types : uint8_t
{
    none = 0,           // No database backend
    postgres = 1,       // PostgreSQL backend  
    mysql = 2,          // MySQL/MariaDB backend
    sqlite = 3,         // SQLite backend
    oracle = 4,         // Oracle backend (future)
    mongodb = 5         // MongoDB backend (future)
};
```

## Usage Examples

### Basic Database Operations

```cpp
#include <database/database_manager.h>
#include <database/postgres_manager.h>
using namespace database_module;

// Create database manager
auto db_manager = std::make_shared<database_manager>();

// Set PostgreSQL backend
db_manager->set_database_type(database_types::postgres);

// Connect to database
std::string connection_string = "host=localhost port=5432 dbname=messaging user=admin password=secret";
bool connected = db_manager->connect(connection_string);

if (connected) {
    std::cout << "✓ Connected to PostgreSQL database" << std::endl;
}
```

### Table Management

```cpp
// Create table with SQL
std::string create_table_sql = R"(
    CREATE TABLE IF NOT EXISTS users (
        id SERIAL PRIMARY KEY,
        username VARCHAR(50) UNIQUE NOT NULL,
        email VARCHAR(100) UNIQUE NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        metadata JSONB
    )
)";

bool table_created = db_manager->create_query(create_table_sql);
if (table_created) {
    std::cout << "✓ Users table created successfully" << std::endl;
}

// Drop table if needed
bool table_dropped = db_manager->drop_query("DROP TABLE IF EXISTS temp_table");
```

### Data Operations

```cpp
// Insert data
std::string insert_sql = R"(
    INSERT INTO users (username, email, metadata) 
    VALUES ('john_doe', 'john@example.com', '{"role": "admin", "active": true}')
)";

bool insert_success = db_manager->insert_query(insert_sql);

// Select data
std::string select_sql = "SELECT id, username, email FROM users WHERE username = 'john_doe'";
auto select_result = db_manager->select_query(select_sql);

if (select_result) {
    std::cout << "Found user: " << *select_result << std::endl;
}

// Update data
std::string update_sql = "UPDATE users SET email = 'john.doe@example.com' WHERE username = 'john_doe'";
bool update_success = db_manager->update_query(update_sql);

// Delete data
std::string delete_sql = "DELETE FROM users WHERE username = 'john_doe'";
bool delete_success = db_manager->delete_query(delete_sql);
```

## Building

The Database module is built as part of the main system:

```bash
# Build with PostgreSQL support (default)
mkdir build && cd build
cmake .. -DUSE_POSTGRESQL=ON
make

# Build database tests
make database_test
./bin/database_test
```

## Dependencies

- **C++20 Standard Library**: Required for concepts, ranges, and coroutines
- **libpqxx**: PostgreSQL C++ library (when PostgreSQL is enabled)
- **OpenSSL**: For secure database connections
- **Threads**: For concurrent database operations

### vcpkg Dependencies

```bash
# Install required packages
vcpkg install libpqxx openssl
```

## License

BSD 3-Clause License - see main project LICENSE file.
