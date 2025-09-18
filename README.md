# Database System

Advanced C++20 Database System with Multi-Backend Support and High-Performance Operations

## Overview

The Database System provides a comprehensive database abstraction layer with support for multiple database backends. Based on the architecture from messaging_system, it features modular design, connection pooling, prepared statements, transaction management, and thread-safe operations optimized for high-performance applications.

## Features

### 🎯 Core Capabilities
- **Multi-Backend Support**: PostgreSQL, MySQL, SQLite support with unified interface (future implementations)
- **Modular Architecture**: Clean separation with database module directory structure
- **Thread Safety**: Concurrent database operations with proper synchronization
- **Independent Design**: No external container dependencies - uses standard C++ types
- **Modern C++**: C++20 standard with concepts, variants, and modern patterns
- **Flexible Build**: Optional PostgreSQL support with graceful fallbacks

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

## Architecture

```
database_system/
├── database/                    # Database module
│   ├── database_base.h         # Abstract base class
│   ├── database_manager.h      # Singleton manager
│   ├── database_types.h        # Type definitions
│   ├── postgres_manager.h      # PostgreSQL implementation
│   └── CMakeLists.txt          # Module build configuration
├── samples/                     # Usage examples
├── tests/                       # Unit tests
└── CMakeLists.txt              # Main build configuration
```

### Data Types

The system uses modern C++ types for database results:

```cpp
// Database result types for independent operation
using database_value = std::variant<std::string, int64_t, double, bool, std::nullptr_t>;
using database_row = std::map<std::string, database_value>;
using database_result = std::vector<database_row>;
```

## Usage Examples

### Basic Database Operations

```cpp
#include <database/database_manager.h>
#include <database/postgres_manager.h>
using namespace database;

int main() {
    // Get singleton instance
    database_manager& db = database_manager::handle();

    // Configure database type
    if (!db.set_mode(database_types::postgres)) {
        std::cerr << "Failed to set database mode" << std::endl;
        return 1;
    }

    // Connect to database
    std::string connection_string =
        "host=localhost port=5432 dbname=test_db user=admin password=secret";

    if (!db.connect(connection_string)) {
        std::cerr << "Failed to connect to database" << std::endl;
        return 1;
    }

    // Execute queries
    bool success = db.create_query(
        "CREATE TABLE IF NOT EXISTS users ("
        "id SERIAL PRIMARY KEY, "
        "username VARCHAR(50), "
        "email VARCHAR(100)"
        ")"
    );

    if (success) {
        std::cout << "Table created successfully" << std::endl;
    }

    return 0;
}
```

### Working with Results

```cpp
// INSERT data
unsigned int inserted = db.insert_query(
    "INSERT INTO users (username, email) "
    "VALUES ('john_doe', 'john@example.com')"
);
std::cout << "Inserted " << inserted << " rows" << std::endl;

// SELECT data
database_result users = db.select_query("SELECT * FROM users");

for (const auto& row : users) {
    for (const auto& [column, value] : row) {
        std::cout << column << ": ";
        std::visit([](const auto& v) {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::nullptr_t>) {
                std::cout << "NULL";
            } else {
                std::cout << v;
            }
        }, value);
        std::cout << " ";
    }
    std::cout << std::endl;
}
```

## Building

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- CMake 3.16+
- Optional: PostgreSQL development libraries (libpq-dev or libpqxx)

### Build Options

```bash
# Build with PostgreSQL support (requires libpqxx)
mkdir build && cd build
cmake .. -DUSE_POSTGRESQL=ON
make

# Build without PostgreSQL (uses mock implementation)
cmake .. -DUSE_POSTGRESQL=OFF
make

# Build with samples and tests
cmake .. -DBUILD_DATABASE_SAMPLES=ON -DUSE_UNIT_TEST=ON
make
```

### vcpkg Dependencies (Optional)

```bash
# For PostgreSQL support
vcpkg install libpqxx openssl

# For future MySQL support
vcpkg install mysql-connector-cpp

# For future SQLite support
vcpkg install sqlite3
```

## Configuration

### Environment Variables

```bash
# PostgreSQL connection settings
export DB_HOST=localhost
export DB_PORT=5432
export DB_NAME=database_system
export DB_USER=app_user
export DB_PASSWORD=secure_password
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `USE_POSTGRESQL` | ON | Enable PostgreSQL support |
| `USE_MYSQL` | OFF | Enable MySQL support (future) |
| `USE_SQLITE` | OFF | Enable SQLite support (future) |
| `BUILD_DATABASE_SAMPLES` | ON | Build sample programs |
| `USE_UNIT_TEST` | ON | Build unit tests |
| `BUILD_SHARED_LIBS` | OFF | Build as shared library |

## Key Improvements from messaging_system

### ✅ Architectural Enhancements
- **Modular Structure**: Clean database module separation following messaging_system patterns
- **Independent Types**: No external container dependencies - uses standard C++ containers
- **Flexible PostgreSQL Support**: Builds with or without PostgreSQL libraries
- **Enhanced Type System**: Rich variant-based result types with proper type conversion

### ✅ Build System
- **Optional Dependencies**: PostgreSQL support can be disabled for testing
- **Modern CMake**: Clean, maintainable build configuration
- **Platform Support**: Windows, macOS, Linux compatibility

### ✅ API Design
- **Consistent Interface**: Unified database_base abstraction
- **Type Safety**: Strong typing with database_types enum
- **Resource Management**: RAII-based connection management
- **Error Handling**: Graceful fallbacks for missing dependencies

## Testing

```bash
# Run all tests
ctest

# Run specific test
./bin/database_test

# Run samples
./bin/basic_usage
./bin/postgres_advanced  # Requires PostgreSQL
```

## Migration Guide

If migrating from the original database_system:

1. **Headers**: Include from `database/` subdirectory
2. **Types**: Use `database_result` instead of container types
3. **Namespace**: Use `database` namespace
4. **Build**: Use new CMake options for PostgreSQL

```cpp
// Old way
#include "database_manager.h"
using namespace database_module;

// New way
#include "database/database_manager.h"
using namespace database;
```

## Future Enhancements

- **Multi-database Support**: MySQL, SQLite, SQL Server drivers
- **Connection Pooling**: Advanced pooling with load balancing
- **Query Builder**: Type-safe query construction
- **ORM Features**: Object-relational mapping capabilities
- **Async Operations**: Coroutine-based async database operations

## License

BSD 3-Clause License - see main project LICENSE file.

---

**Note**: This system has been upgraded from the messaging_system database module to provide a standalone, independent database abstraction layer suitable for various C++ projects.