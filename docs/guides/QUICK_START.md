# Database System Quick Start Guide

**Version:** 0.1.0
**Last Updated:** 2025-11-11

Get started with database_system in 5 minutes.

## Prerequisites

- C++17 or higher compiler
- CMake 3.16+
- PostgreSQL or SQLite installed (optional for demo)

## Installation

### Method 1: Using vcpkg

```bash
vcpkg install kcenon-database-system[postgresql]:x64-linux
```

### Method 2: Using FetchContent

Add to your `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(database_system
  GIT_REPOSITORY https://github.com/kcenon/database_system.git
  GIT_TAG v0.1.0  # Pin to a specific release tag; do NOT use main
)
FetchContent_MakeAvailable(database_system)

target_link_libraries(your_target database_system::database)
```

### Method 3: Build from Source

```bash
git clone https://github.com/kcenon/database_system.git
cd database_system
mkdir build && cd build
cmake .. -DUSE_POSTGRESQL=ON
cmake --build . --config Release
cmake --install .
```

## First Program

Create `main.cpp`:

```cpp
#include "integrated/unified_database_system.h"
#include <iostream>

using namespace database::integrated;

int main() {
    // Create database instance with logging and monitoring
    auto db = unified_database_system::create_builder()
        .enable_logging(db_log_level::info, "./logs")
        .enable_monitoring(true)
        .set_pool_size(2, 10)
        .build();

    if (!db) {
        std::cerr << "Failed to create database\n";
        return 1;
    }

    std::cout << "Database initialized successfully!\n";

    // Check health status
    auto health = db->check_health();
    std::cout << "Status: " << (health.is_connected ? "Connected" : "Not connected") << "\n";

    return 0;
}
```

Compile with CMake:

```bash
cmake .. -DBUILD_DATABASE_SAMPLES=ON
cmake --build .
./your_executable
```

## Backend Selection Example

```cpp
auto db = unified_database_system::create_builder()
    .set_backend_type(backend_type::postgresql)  // or sqlite
    .enable_logging(db_log_level::info, "./logs")
    .set_pool_size(5, 20)
    .build();

auto result = db->connect("host=localhost dbname=mydb user=admin password=secret");
if (result.is_ok()) {
    std::cout << "Connected successfully\n";
} else {
    std::cerr << "Connection error: " << result.error().message << "\n";
}
```

## Connection Pooling Example

```cpp
// Configure connection pool (min_size, max_size)
auto db = unified_database_system::create_builder()
    .set_pool_size(5, 50)          // 5-50 connections
    .set_connection_timeout(30s)   // 30 second timeout
    .build();

// Pool automatically manages connections
auto query_result = db->execute("SELECT * FROM users");

// Check pool utilization via metrics
auto metrics = db->get_metrics();
std::cout << "Active connections: " << metrics.active_connections << "\n";
std::cout << "Pool utilization: " << (metrics.pool_utilization * 100) << "%\n";
```

## Simple Query Example

```cpp
// Execute SELECT query
auto result = db->execute("SELECT id, name FROM users WHERE active = true");

if (result.is_ok()) {
    auto rows = result.value();
    for (const auto& row : rows) {
        for (const auto& [column, value] : row) {
            std::cout << column << ": " << value << " ";
        }
        std::cout << "\n";
    }
} else {
    std::cerr << "Query error: " << result.error().message << "\n";
}
```

## Next Steps

- [API Reference](../API_REFERENCE.md) - Comprehensive API documentation
- [Architecture Guide](../ARCHITECTURE.md) - System design and patterns
- [Integration Guide](../../INTEGRATION.md) - Advanced integration scenarios
- [FAQ](./FAQ.md) - Frequently asked questions
- [Examples](../../samples/) - Complete sample applications

## Common Issues

**Database not connecting?**
- Ensure database server is running
- Verify connection string format: `host=localhost dbname=db user=user password=pass`
- Check database user permissions

**CMake not finding dependencies?**
- Install dependencies: `vcpkg install libpq openssl:x64-linux`
- Set vcpkg CMAKE_TOOLCHAIN_FILE: `-DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake`

**Compilation errors with C++17?**
- Update compiler: GCC 7+, Clang 5+, or MSVC 2017+
- Ensure `-std=c++17` is set in CMakeLists.txt

## Support

- GitHub Issues: https://github.com/kcenon/database_system/issues
- Documentation: https://github.com/kcenon/database_system/wiki
- Email: [project maintainer]
