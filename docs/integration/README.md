---
doc_id: "DBS-GUID-021"
doc_title: "Database System Integration Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "GUID"
---

# Database System Integration Guide

> **SSOT**: This document is the single source of truth for **Database System Integration Guide**.

## Overview

This directory contains integration guides for using database_system with other KCENON systems.

## Integration Guides

- [With Common System](with-common-system.md) - Foundation interfaces and error handling
- [With Logger System](with-logger.md) - Database query and error logging
- [With Monitoring System](with-monitoring.md) - Database metrics and performance monitoring
- [With Thread System](with-thread-system.md) - Connection pooling and async queries
- [With Network System](with-network-system.md) - Network-based database protocols

## Quick Start

### Basic Database Connection with Logging

```cpp
#include "database_system/Database.h"
#include "logger_system/Logger.h"

int main() {
    auto logger = logger_system::createLogger("database.log");

    auto db = database_system::Database::connect({
        .host = "localhost",
        .port = 5432,
        .database = "mydb",
        .logger = logger
    });

    if (!db) {
        logger->error("Connection failed: {}", db.error());
        return 1;
    }

    auto result = db->query("SELECT * FROM users");
    if (result) {
        logger->info("Found {} users", result->size());
    }
}
```

### Connection Pool with Full Observability

```cpp
#include "database_system/ConnectionPool.h"
#include "logger_system/Logger.h"
#include "monitoring_system/Monitor.h"

int main() {
    auto logger = logger_system::createLogger("db.log");
    auto monitor = monitoring_system::createMonitor();

    auto pool = database_system::ConnectionPool::create({
        .connection_string = "postgresql://localhost/mydb",
        .min_connections = 5,
        .max_connections = 20,
        .logger = logger,
        .monitor = monitor
    });

    // Pool automatically tracks:
    // - Active connections
    // - Query duration
    // - Error rates
    // - Connection wait time

    auto conn = pool->acquire();
    auto result = conn->query("SELECT * FROM users WHERE id = $1", user_id);
}
```

## Integration Patterns

### Dependency Injection

```cpp
class UserRepository {
public:
    UserRepository(
        std::shared_ptr<database_system::ConnectionPool> pool,
        std::shared_ptr<ILogger> logger)
        : pool_(std::move(pool))
        , logger_(std::move(logger))
    {}

    Result<User> findById(int id) {
        auto conn = TRY(pool_->acquire());
        auto result = TRY(conn->query("SELECT * FROM users WHERE id = $1", id));

        if (result.empty()) {
            return Error("User not found");
        }

        return Ok(User::from_row(result[0]));
    }

private:
    std::shared_ptr<database_system::ConnectionPool> pool_;
    std::shared_ptr<ILogger> logger_;
};
```

### Error Handling with Result<T>

```cpp
Result<std::vector<User>> getAllUsers() {
    auto conn = TRY(pool->acquire());
    auto result = TRY(conn->query("SELECT * FROM users"));

    std::vector<User> users;
    for (const auto& row : result) {
        users.push_back(User::from_row(row));
    }

    return Ok(users);
}
```

### Async Queries with Coroutines

```cpp
async_result<User> fetchUserAsync(int id) {
    auto conn = co_await pool->acquire_async();
    auto result = co_await conn->query_async("SELECT * FROM users WHERE id = $1", id);

    if (result.empty()) {
        co_return Error("User not found");
    }

    co_return Ok(User::from_row(result[0]));
}
```

## Common Use Cases

### 1. ORM Integration

Dependencies: `common_system`, `logger_system`, `container_system`

```cpp
class User : public database_system::Model<User> {
public:
    DEFINE_TABLE("users");
    DEFINE_FIELD(int, id, PRIMARY_KEY);
    DEFINE_FIELD(std::string, username);
    DEFINE_FIELD(std::string, email);
};

auto user = User::find(123);  // Find by primary key
auto users = User::where("email LIKE $1", "%@example.com");
```

### 2. Migration Management

```cpp
auto migrator = database_system::Migrator::create(db, logger);

migrator->addMigration("001_create_users", R"(
    CREATE TABLE users (
        id SERIAL PRIMARY KEY,
        username VARCHAR(255) NOT NULL,
        email VARCHAR(255) UNIQUE NOT NULL
    );
)");

migrator->run();
```

### 3. Transaction Management

```cpp
auto transaction = TRY(conn->begin_transaction());

auto user_result = TRY(transaction->query(
    "INSERT INTO users (username, email) VALUES ($1, $2) RETURNING id",
    "john_doe", "john@example.com"
));

auto user_id = user_result[0]["id"].as_int();

TRY(transaction->query(
    "INSERT INTO user_profiles (user_id, bio) VALUES ($1, $2)",
    user_id, "Software developer"
));

TRY(transaction->commit());
```

## Performance Considerations

- Configure connection pool size based on workload
- Use prepared statements for frequently executed queries
- Enable query result caching for read-heavy workloads
- Monitor slow queries with monitoring_system integration

## Security

- Always use parameterized queries to prevent SQL injection
- Never log sensitive data (passwords, tokens)
- Use SSL/TLS for database connections in production
- Implement proper access control and permissions

## Additional Resources

- [Database System API Reference](../API_REFERENCE.md)
- [Database System Architecture](../ARCHITECTURE.md)
- [Ecosystem Integration Guide](../../../ECOSYSTEM.md)
- [Example Applications](../../../examples/database/)

## Support

For questions or issues:
1. Check system-specific documentation
2. Review [TROUBLESHOOTING guide](../guides/TROUBLESHOOTING.md)
3. File an issue in the database_system repository
