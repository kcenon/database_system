# Database System - Frequently Asked Questions

**Version:** 0.1.0
**Last Updated:** 2025-11-11
**Status:** Stable

---

## Table of Contents

- [Installation & Setup](#installation--setup)
- [Basic Usage](#basic-usage)
- [Backend Selection](#backend-selection)
- [Connection Pooling](#connection-pooling)
- [ORM Framework](#orm-framework)
- [Transactions](#transactions)
- [Security](#security)
- [Performance](#performance)
- [Troubleshooting](#troubleshooting)
- [Integration](#integration)
- [Advanced Topics](#advanced-topics)
- [Platform-Specific](#platform-specific)

---

## Installation & Setup

### Q: What are the minimum requirements?

**A:** database_system requires:
- C++ compiler with C++17 support or higher (C++20 for async features)
- CMake 3.16 or higher
- Optional: Database client libraries (PostgreSQL, MySQL, SQLite, MongoDB, Redis)

Supported platforms:
- macOS 10.15+
- Ubuntu 20.04+
- Windows 10+

Supported compilers:
- GCC 7+ (C++17), GCC 10+ (C++20 with coroutines)
- Clang 5+ (C++17), Clang 11+ (C++20 with coroutines)
- MSVC 2017+ (C++17), MSVC 2019+ (C++20 with coroutines)

### Q: How do I install database_system?

**A:** There are several installation methods:

**Method 1: Using vcpkg**
```bash
# Install vcpkg first
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh

# Install database dependencies (optional)
vcpkg install libpqxx openssl  # PostgreSQL
vcpkg install libmysql          # MySQL
vcpkg install sqlite3           # SQLite
vcpkg install mongo-cxx-driver  # MongoDB
vcpkg install hiredis           # Redis
```

**Method 2: Building from source**
```bash
git clone https://github.com/kcenon/database_system.git
cd database_system
mkdir build && cd build
cmake .. -DUSE_POSTGRESQL=ON -DUSE_MYSQL=ON -DUSE_SQLITE=ON
cmake --build .
sudo cmake --install .
```

**Method 3: FetchContent in CMake**
```cmake
include(FetchContent)
FetchContent_Declare(
    database_system
    GIT_REPOSITORY https://github.com/kcenon/database_system.git
    GIT_TAG main
)
FetchContent_MakeAvailable(database_system)
target_link_libraries(your_target PRIVATE database_system::database)
```

### Q: Build fails with "C++17 required" error. What should I do?

**A:** Your compiler may not support C++17. Update your compiler:

- **macOS**: `xcode-select --install` or `brew install llvm`
- **Ubuntu**: `sudo apt install g++-7` (or higher)
- **Windows**: Install Visual Studio 2017 or higher

Then specify the compiler in CMake:
```bash
cmake -DCMAKE_CXX_COMPILER=g++-7 ..
```

### Q: Can I build without any database backends?

**A:** Yes! database_system works with mock implementations for testing:

```bash
cmake .. \
  -DUSE_POSTGRESQL=OFF \
  -DUSE_MYSQL=OFF \
  -DUSE_SQLITE=OFF \
  -DUSE_MONGODB=OFF \
  -DUSE_REDIS=OFF
ninja
```

This is useful for CI/CD pipelines and unit testing without actual databases.

---

## Basic Usage

### Q: How do I get started with database_system?

**A:** The simplest way is using `unified_database_system`:

```cpp
#include <database/integrated/unified_database_system.h>

using namespace database::integrated;

int main() {
    // Zero-config initialization
    auto db = unified_database_system::create_builder()
        .enable_logging(db_log_level::info, "./logs")
        .enable_monitoring(true)
        .set_pool_size(2, 10)
        .build();

    if (!db) {
        std::cerr << "Failed to create database instance\n";
        return 1;
    }

    // Connect to database
    auto connect_result = db->connect(
        "host=localhost dbname=mydb user=admin password=secret"
    );

    if (!connect_result.is_ok()) {
        std::cerr << "Connection failed: " << connect_result.error().message << "\n";
        return 1;
    }

    // Execute query
    auto result = db->execute("SELECT * FROM users WHERE age > $1", {25});

    if (result.is_ok()) {
        std::cout << "Found " << result.value().size() << " users\n";
    }

    return 0;
}
```

For more details, see [Quick Start Guide](../BUILD_GUIDE.md).

### Q: What are the most common use cases?

**A:** The database_system is designed for:

1. **Enterprise web applications**: Multi-tenant apps with complex data models
   - Connection pooling for 10,000+ concurrent users
   - Transaction management for data integrity
   - Performance monitoring for optimization

2. **Financial systems**: High-frequency trading and ACID requirements
   - Distributed transactions across multiple databases
   - Real-time performance metrics
   - Security audit logging

3. **IoT platforms**: Time-series data storage with real-time analytics
   - MongoDB for document storage
   - Redis for caching and pub/sub
   - PostgreSQL for relational analytics

4. **Content management**: Large-scale content storage and retrieval
   - BLOB storage with serialization
   - Full-text search (MySQL, PostgreSQL)
   - GridFS for large files (MongoDB)

5. **Gaming platforms**: Player data persistence with real-time features
   - Redis for leaderboards and session management
   - PostgreSQL for player profiles
   - Connection pooling for scalability

### Q: Can I use database_system in a commercial project?

**A:** Yes, database_system is licensed under BSD 3-Clause license, which allows commercial use. See [LICENSE](../../LICENSE) for details.

### Q: What's the difference between unified_database_system and database_manager?

**A:** Two APIs are available:

**unified_database_system (Recommended)**:
- Zero-config with builder pattern
- Integrated logging, monitoring, threading
- Modern Result<T> error handling
- Async operations built-in
- Best for new projects

```cpp
auto db = unified_database_system::create_builder()
    .set_pool_size(10, 50)
    .enable_logging(db_log_level::info)
    .build();
```

**database_manager (Legacy)**:
- Singleton pattern (now supports DI)
- Manual adapter setup
- Lower-level control
- Backward compatible
- Use for existing code migration

```cpp
auto context = std::make_shared<database_context>();
auto db_mgr = std::make_shared<database_manager>(context);
```

---

## Backend Selection

### Q: Which database backends are supported?

**A:** database_system supports 5 backends:

| Backend | Use Case | Performance | ORM | TLS/SSL |
|---------|----------|-------------|-----|---------|
| **PostgreSQL** | OLTP, Analytics | Excellent | ✅ | ✅ |
| **MySQL** | Web apps, Full-text | Very Good | ✅ | ✅ |
| **SQLite** | Embedded, Testing | Good | ✅ | ✅ |
| **MongoDB** | Documents, NoSQL | Very Good | ✅ | ✅ |
| **Redis** | Caching, Sessions | Excellent | ✅ | ✅ |

### Q: How do I select a database backend?

**A:** Backends are selected at build time and runtime:

**Build-time configuration**:
```bash
cmake .. -DUSE_POSTGRESQL=ON -DUSE_REDIS=ON
```

**Runtime selection** (using backend registry):
```cpp
#include <database/core/backend_registry.h>

using namespace database;

// Create backend dynamically
auto registry = backend_registry::instance();
auto backend_result = registry.create("postgresql");

if (backend_result.is_ok()) {
    auto backend = std::move(backend_result.value());

    // Configure connection
    connection_config config;
    config.host = "localhost";
    config.port = 5432;
    config.database = "mydb";
    config.username = "admin";
    config.password = "secret";

    auto connect_result = backend->connect(config);
}
```

### Q: Can I use multiple database backends simultaneously?

**A:** Yes! The connection pool supports multiple backends:

```cpp
#include <database/database_manager.h>

using namespace database;

auto context = std::make_shared<database_context>();
auto db = std::make_shared<database_manager>(context);

// PostgreSQL for OLTP operations
connection_pool_config pg_config;
pg_config.connection_string = "host=localhost dbname=oltp_db";
db->create_connection_pool(database_types::postgres, pg_config);

// Redis for caching
connection_pool_config redis_config;
redis_config.connection_string = "redis://localhost:6379/0";
db->create_connection_pool(database_types::redis, redis_config);

// Use PostgreSQL
db->set_mode(database_types::postgres);
auto users = db->select_query("SELECT * FROM users WHERE id = 12345");

// Cache in Redis
db->set_mode(database_types::redis);
db->execute_query("SET user:12345 '{\"name\":\"John\",\"email\":\"john@example.com\"}'");
```

### Q: How do I migrate from one backend to another?

**A:** Use the unified API to minimize migration effort:

1. **Update connection string** in configuration
2. **Test queries** with new backend
3. **Migrate data** using export/import
4. **Update backend-specific features** (e.g., JSONB in PostgreSQL)

Example migration helper:
```cpp
void migrate_data(database_manager& source_db, database_manager& target_db) {
    // Export from source
    source_db.set_mode(database_types::postgres);
    auto data = source_db.select_query("SELECT * FROM users");

    // Import to target
    target_db.set_mode(database_types::mysql);
    for (const auto& row : data) {
        // Convert and insert data
        target_db.execute_query(
            "INSERT INTO users (id, name, email) VALUES (?, ?, ?)",
            row.at("id"), row.at("name"), row.at("email")
        );
    }
}
```

---

## Connection Pooling

### Q: How does connection pooling work?

**A:** Connection pooling reuses database connections to minimize overhead:

**Key Features**:
- **0.1ms** average connection acquisition time (20x faster than native)
- **10,000+** concurrent connections supported
- **Health monitoring** with automatic connection validation
- **Thread-safe** with mutex protection
- **Adaptive sizing** based on load

```cpp
#include <database/connection_pool.h>

using namespace database;

// Configure connection pool
connection_pool_config config;
config.min_connections = 5;              // Minimum connections
config.max_connections = 50;             // Maximum connections
config.acquire_timeout = std::chrono::seconds(5);
config.idle_timeout = std::chrono::seconds(30);
config.health_check_interval = std::chrono::seconds(60);
config.enable_health_checks = true;
config.connection_string = "host=localhost dbname=mydb";

// Create pool
auto db = std::make_shared<database_manager>(context);
bool pool_created = db->create_connection_pool(database_types::postgres, config);

// Acquire connection (RAII-managed)
auto pool = db->get_connection_pool(database_types::postgres);
auto connection = pool->acquire_connection();

if (connection) {
    // Use connection (automatically returned to pool when goes out of scope)
    auto result = connection->select_query("SELECT * FROM users");
}
```

### Q: How do I monitor connection pool performance?

**A:** Use pool statistics API:

```cpp
// Get pool statistics
auto stats = db->get_pool_stats();

for (const auto& [db_type, stat] : stats) {
    std::cout << "Backend: " << static_cast<int>(db_type) << "\n";
    std::cout << "  Active connections: " << stat.active_connections << "\n";
    std::cout << "  Available connections: " << stat.available_connections << "\n";
    std::cout << "  Total connections: "
              << (stat.active_connections + stat.available_connections) << "\n";

    // Calculate utilization
    double utilization = (double)stat.active_connections /
                        (stat.active_connections + stat.available_connections);
    std::cout << "  Utilization: " << (utilization * 100) << "%\n";
}
```

### Q: What happens when the connection pool is exhausted?

**A:** The pool handles exhaustion gracefully:

1. **Wait for available connection** (up to `acquire_timeout`)
2. **Create new connection** (if below `max_connections`)
3. **Return timeout error** (if timeout expires)

```cpp
// Configure timeout
config.acquire_timeout = std::chrono::seconds(5);

// Acquire connection
auto connection = pool->acquire_connection();

if (!connection) {
    std::cerr << "Pool exhausted - all connections in use\n";
    // Handle error: retry, queue request, or reject
}
```

**Best Practices**:
- Set `max_connections` based on database server limits
- Monitor pool utilization (target: 60-80%)
- Use connection pooling in all multi-threaded applications
- Always release connections promptly (RAII handles this automatically)

### Q: How do I tune connection pool size?

**A:** Follow these guidelines:

**Formula**: `optimal_pool_size = (core_count * 2) + effective_spindle_count`

For typical web applications:
```cpp
// Development (4 cores, SSD)
config.min_connections = 2;
config.max_connections = 10;

// Production (16 cores, SSD)
config.min_connections = 10;
config.max_connections = 50;

// High-traffic (32 cores, SSD array)
config.min_connections = 20;
config.max_connections = 100;
```

**Monitoring-based tuning**:
1. Start with conservative values
2. Monitor pool utilization
3. Increase if utilization > 80%
4. Decrease if utilization < 40%

---

## ORM Framework

### Q: Does database_system provide ORM support?

**A:** Yes! The ORM framework provides type-safe entity mapping:

```cpp
#include <database/orm/entity.h>

// Define entity class
class User : public entity_base {
public:
    int64_t id;
    std::string username;
    std::string email;
    std::chrono::system_clock::time_point created_at;
    bool is_active;

    // Metadata definition
    static std::string get_table_name() { return "users"; }

    static std::vector<field_info> get_fields() {
        return {
            {"id", field_type::integer, {.primary_key = true, .auto_increment = true}},
            {"username", field_type::text, {.not_null = true, .unique = true}},
            {"email", field_type::text, {.not_null = true, .unique = true}},
            {"created_at", field_type::timestamp, {.default_now = true}},
            {"is_active", field_type::boolean, {.default_value = "true"}}
        };
    }
};
```

### Q: How do I perform CRUD operations with ORM?

**A:** Use entity_manager for type-safe operations:

```cpp
#include <database/orm/entity_manager.h>

auto context = std::make_shared<database_context>();
auto entity_mgr = context->get_entity_manager();
auto db = std::make_shared<database_manager>(context);

// Create table (automatic schema generation)
entity_mgr->create_table<User>(db.get());

// Create (INSERT)
User new_user;
new_user.username = "john_doe";
new_user.email = "john@example.com";
new_user.is_active = true;

bool inserted = entity_mgr->save(db.get(), new_user);

// Read (SELECT)
auto users = entity_mgr->find_all<User>(db.get());
for (const auto& user : users) {
    std::cout << "User: " << user.username << " <" << user.email << ">\n";
}

// Update
new_user.email = "newemail@example.com";
bool updated = entity_mgr->update(db.get(), new_user);

// Delete
bool deleted = entity_mgr->remove(db.get(), new_user.id);
```

### Q: Can I use custom queries with ORM?

**A:** Yes, combine ORM with query builder:

```cpp
// Custom query with ORM mapping
auto active_users = entity_mgr->query<User>(db.get())
    .where("is_active = ?", true)
    .where("created_at > ?", last_month)
    .order_by("username", sort_order::asc)
    .limit(100)
    .execute();

for (const auto& user : active_users) {
    std::cout << user.username << "\n";
}
```

---

## Transactions

### Q: How do I use transactions?

**A:** database_system provides ACID-compliant transactions:

```cpp
#include <database/database_manager.h>

auto db = std::make_shared<database_manager>(context);
db->connect("host=localhost dbname=mydb");

// Begin transaction
bool tx_started = db->begin_transaction();

if (tx_started) {
    try {
        // Execute multiple operations
        db->execute_query("INSERT INTO users (name) VALUES ('Alice')");
        db->execute_query("UPDATE accounts SET balance = balance - 100 WHERE user_id = 1");
        db->execute_query("UPDATE accounts SET balance = balance + 100 WHERE user_id = 2");

        // Commit transaction
        bool committed = db->commit();

        if (committed) {
            std::cout << "Transaction committed successfully\n";
        }
    } catch (const std::exception& e) {
        // Rollback on error
        db->rollback();
        std::cerr << "Transaction failed: " << e.what() << "\n";
    }
}
```

### Q: Are distributed transactions supported?

**A:** Yes! Use transaction_coordinator for distributed transactions:

```cpp
#include <database/async/async_operations.h>

auto context = std::make_shared<database_context>();
auto tx_coord = context->get_transaction_coordinator();

// Begin distributed transaction across multiple databases
auto tx_id = tx_coord->begin_distributed_transaction({db1, db2, db3});

// Execute operations on different databases
db1->execute_query("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
db2->execute_query("INSERT INTO transactions (amount) VALUES (100)");
db3->execute_query("UPDATE ledger SET total = total + 100");

// Two-phase commit
auto commit_result = tx_coord->commit_distributed_transaction(tx_id);

if (commit_result) {
    std::cout << "Distributed transaction committed\n";
} else {
    std::cout << "Distributed transaction rolled back\n";
}
```

### Q: What isolation levels are supported?

**A:** Isolation levels depend on the backend:

**PostgreSQL**:
- READ UNCOMMITTED
- READ COMMITTED (default)
- REPEATABLE READ
- SERIALIZABLE

```cpp
// Set isolation level
db->execute_query("SET TRANSACTION ISOLATION LEVEL SERIALIZABLE");
db->begin_transaction();
// ... transaction operations
db->commit();
```

**MySQL**:
- READ UNCOMMITTED
- READ COMMITTED
- REPEATABLE READ (default)
- SERIALIZABLE

**SQLite**:
- DEFERRED
- IMMEDIATE
- EXCLUSIVE

---

## Security

### Q: How does database_system handle credentials?

**A:** Use credential_manager for secure storage:

```cpp
#include <database/security/secure_connection.h>

auto context = std::make_shared<database_context>();
auto cred_mgr = context->get_credential_manager();

// Store credentials with encryption
security_credentials creds;
creds.username = "admin";
creds.password_hash = cred_mgr->hash_password("secure_password");
creds.encryption = encryption_type::tls;
creds.verify_certificate = true;

cred_mgr->store_credentials("prod_db", creds);

// Retrieve credentials
auto stored_creds = cred_mgr->get_credentials("prod_db");
if (stored_creds) {
    std::cout << "Username: " << stored_creds->username << "\n";
}
```

**Security Features**:
- Master key encryption for credentials
- Password hashing with bcrypt
- TLS/SSL connection encryption
- Certificate verification
- Session timeout management

### Q: What is RBAC and how do I use it?

**A:** Role-Based Access Control (RBAC) controls database permissions:

```cpp
#include <database/security/secure_connection.h>

auto context = std::make_shared<database_context>();
auto access_ctrl = context->get_access_control();

// Define roles
access_control::role read_only_role;
read_only_role.name = "read_only";
read_only_role.permissions = access_control::permission::select;

access_control::role admin_role;
admin_role.name = "admin";
admin_role.permissions =
    access_control::permission::select |
    access_control::permission::insert |
    access_control::permission::update |
    access_control::permission::delete;

// Create roles
access_ctrl->create_role(read_only_role);
access_ctrl->create_role(admin_role);

// Assign roles to users
access_ctrl->assign_role_to_user("analyst", "read_only");
access_ctrl->assign_role_to_user("dba", "admin");

// Check permissions
bool can_delete = access_ctrl->check_permission(
    "analyst",
    access_control::permission::delete
);
// Returns false - analyst only has SELECT permission
```

### Q: How do I enable audit logging?

**A:** Use audit_logger for security event tracking:

```cpp
#include <database/security/secure_connection.h>

// Audit logging is automatic for all database operations
// Manual logging example:
AUDIT_LOG_ACCESS(
    "admin_user",           // user_id
    "session_abc123",       // session_id
    "DELETE",               // operation
    "users",                // table
    "WHERE id > 1000",      // query_hash
    true,                   // success
    ""                      // error_message
);

// Configure audit logger
auto context = std::make_shared<database_context>();
auto audit_logger = context->get_audit_logger();

// Set audit log retention
audit_logger->set_retention_days(90);

// Export audit log
auto logs = audit_logger->get_logs_since(last_week);
for (const auto& log : logs) {
    std::cout << log.timestamp << ": " << log.user_id
              << " " << log.operation << " " << log.table << "\n";
}
```

### Q: How do I prevent SQL injection?

**A:** Always use prepared statements or query builders:

```cpp
// ❌ UNSAFE - SQL injection vulnerability
std::string user_input = "'; DROP TABLE users; --";
db->execute_query("SELECT * FROM users WHERE name = '" + user_input + "'");

// ✅ SAFE - Prepared statement with parameters
db->execute_query("SELECT * FROM users WHERE name = $1", {user_input});

// ✅ SAFE - Query builder with automatic escaping
auto result = db->create_query_builder(database_types::postgres)
    .select({"id", "name", "email"})
    .from("users")
    .where("name", "=", database_value{user_input})
    .execute(db.get());
```

---

## Performance

### Q: What is the performance of database_system?

**A:** Performance benchmarks (Intel i7-9750H, 16GB RAM, SSD):

**Connection Pooling**:
- **0.1ms** average connection acquisition time
- **20x faster** than native driver connection creation
- **10,000+ concurrent connections** supported

**Query Performance**:
- Simple SELECT: **1.2ms** (PostgreSQL), **0.8ms** (SQLite), **0.3ms** (Redis)
- Complex JOIN: **15ms** (PostgreSQL), **12ms** (SQLite)
- Bulk INSERT (1K records): **45ms** (PostgreSQL), **38ms** (SQLite), **28ms** (Redis)

**Concurrent Operations**:
- **5,000 TPS** transaction throughput (PostgreSQL)
- **95%+ pool efficiency** under load
- **1.16M ops/s** with thread_system integration

See [Performance Benchmarks](../PERFORMANCE_BENCHMARKS.md) for detailed results.

### Q: How can I optimize performance?

**A:** Key optimization tips:

1. **Use Connection Pooling**: 20x faster than creating connections
```cpp
// Configure optimal pool size
config.min_connections = core_count * 2;
config.max_connections = core_count * 4;
```

2. **Use Prepared Statements**: Reduces query parsing overhead
```cpp
// Prepared statement (parsed once, executed many times)
auto stmt = db->prepare("SELECT * FROM users WHERE age > $1");
for (int age : {18, 25, 30, 40}) {
    auto result = stmt->execute({age});
}
```

3. **Batch Operations**: Minimize round-trips
```cpp
// Bulk insert (1 query instead of 1000)
db->begin_transaction();
for (const auto& user : users) {
    db->execute_query("INSERT INTO users (name) VALUES ($1)", {user.name});
}
db->commit();
```

4. **Use Caching**: Redis for frequently accessed data
```cpp
// Check cache first
auto cached = redis_db->execute_query("GET user:12345");
if (cached.empty()) {
    // Cache miss - query database
    auto user = postgres_db->select_query("SELECT * FROM users WHERE id = 12345");
    // Store in cache
    redis_db->execute_query("SET user:12345 '" + serialize(user) + "' EX 3600");
}
```

5. **Monitor Performance**: Use performance_monitor
```cpp
auto context = std::make_shared<database_context>();
auto perf_monitor = context->get_performance_monitor();

// Get performance summary
auto summary = perf_monitor->get_performance_summary();
std::cout << "Queries/sec: " << summary.queries_per_second << "\n";
std::cout << "Avg latency: " << summary.avg_query_time.count() << " μs\n";
std::cout << "Error rate: " << (summary.error_rate * 100) << "%\n";
```

### Q: Is database_system thread-safe?

**A:** Yes! All components are thread-safe:

**Thread-safe components**:
- ✅ Connection pooling (mutex-protected)
- ✅ Database manager (atomic operations)
- ✅ Query execution (per-connection isolation)
- ✅ Performance monitoring (atomic counters)
- ✅ Security components (mutex-protected)

**Concurrency example**:
```cpp
#include <thread>

auto db = std::make_shared<database_manager>(context);
db->create_connection_pool(database_types::postgres, config);

// Spawn 100 threads
std::vector<std::thread> threads;
for (int i = 0; i < 100; ++i) {
    threads.emplace_back([&db, i]() {
        // Each thread acquires its own connection from the pool
        auto pool = db->get_connection_pool(database_types::postgres);
        auto conn = pool->acquire_connection();

        // Thread-safe query execution
        auto result = conn->select_query("SELECT * FROM users WHERE id = " + std::to_string(i));

        // Connection automatically returned to pool
    });
}

for (auto& t : threads) {
    t.join();
}
```

**Best Practices**:
- Use connection pooling in multi-threaded apps
- Let RAII handle connection lifecycle
- Monitor pool utilization under load
- Use distributed transactions for cross-database consistency

---

## Troubleshooting

### Q: I'm getting a compilation error. Where should I start?

**A:** Check these common issues:

1. **Compiler version**: Ensure C++17 support
   ```bash
   g++ --version  # or clang++ --version
   # GCC 7+ or Clang 5+ required for C++17
   # GCC 10+ or Clang 11+ required for C++20 async features
   ```

2. **Include paths**: Verify database_system is in include path
   ```cmake
   find_package(database_system REQUIRED)
   target_link_libraries(your_target PRIVATE database_system::database)
   ```

3. **Dependencies**: Ensure all dependencies are installed
   ```bash
   cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
   # Check if libpqxx, libmysql, sqlite3, etc. are found
   ```

4. **C++ Standard**: Verify C++17 is enabled
   ```cmake
   set(CMAKE_CXX_STANDARD 17)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   ```

If issue persists, see [Build Guide](../BUILD_GUIDE.md) and [Troubleshooting Guide](TROUBLESHOOTING.md).

### Q: Runtime crash with segmentation fault. What should I check?

**A:** Common causes:

1. **Uninitialized database**: Always initialize before use
```cpp
// ❌ WRONG - database not initialized
auto db = std::make_shared<database_manager>(context);
db->execute_query("SELECT * FROM users");  // CRASH

// ✅ CORRECT - initialize first
auto db = std::make_shared<database_manager>(context);
db->set_mode(database_types::postgres);
db->connect("host=localhost dbname=mydb");
db->execute_query("SELECT * FROM users");  // OK
```

2. **Connection pool exhaustion**: Check pool configuration
```cpp
auto stats = db->get_pool_stats();
if (stats.active_connections >= config.max_connections) {
    std::cerr << "Pool exhausted!\n";
}
```

3. **Thread safety violations**: Use connection pooling
```cpp
// ❌ WRONG - sharing connection across threads
auto conn = db->get_connection();
std::thread t1([&conn]() { conn->select_query("..."); });
std::thread t2([&conn]() { conn->select_query("..."); });  // CRASH

// ✅ CORRECT - each thread gets its own connection
auto pool = db->get_connection_pool(database_types::postgres);
std::thread t1([&pool]() {
    auto conn = pool->acquire_connection();
    conn->select_query("...");
});
std::thread t2([&pool]() {
    auto conn = pool->acquire_connection();
    conn->select_query("...");
});
```

Enable AddressSanitizer for debugging:
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
```

### Q: Connection timeout errors. How do I fix this?

**A:** Increase timeout or check database server:

```cpp
// Increase connection timeout
connection_pool_config config;
config.acquire_timeout = std::chrono::seconds(30);  // Default: 5 seconds
config.connection_string = "host=localhost dbname=mydb connect_timeout=10";

// Check database server status
auto health = db->check_health();
if (!health.is_connected) {
    std::cerr << "Database server unreachable\n";
    for (const auto& issue : health.issues) {
        std::cerr << "  Issue: " << issue << "\n";
    }
}

// Monitor connection pool
auto stats = db->get_pool_stats();
std::cout << "Active: " << stats.active_connections << "\n";
std::cout << "Available: " << stats.available_connections << "\n";
```

### Q: Query execution is slow. How do I diagnose this?

**A:** Use performance monitoring:

```cpp
#include <database/monitoring/performance_monitor.h>

auto context = std::make_shared<database_context>();
auto perf_monitor = context->get_performance_monitor();

// Enable performance monitoring
perf_monitor->set_alert_thresholds(
    0.05,                            // 5% error rate threshold
    std::chrono::milliseconds(1000)  // 1 second latency threshold
);

// Register alert handler
perf_monitor->register_alert_handler([](const performance_alert& alert) {
    std::cout << "Performance Alert: " << alert.message() << "\n";
    std::cout << "  Severity: " << static_cast<int>(alert.severity) << "\n";
    std::cout << "  Metric: " << alert.metric_name << "\n";
});

// Get detailed performance summary
auto summary = perf_monitor->get_performance_summary();
std::cout << "Total queries: " << summary.total_queries << "\n";
std::cout << "Avg query time: " << summary.avg_query_time.count() << " μs\n";
std::cout << "Min query time: " << summary.min_query_time.count() << " μs\n";
std::cout << "Max query time: " << summary.max_query_time.count() << " μs\n";
std::cout << "Error rate: " << (summary.error_rate * 100) << "%\n";

// Get per-query metrics
auto query_metrics = perf_monitor->get_query_metrics();
for (const auto& [query_hash, metric] : query_metrics) {
    std::cout << "Query: " << query_hash << "\n";
    std::cout << "  Executions: " << metric.execution_count << "\n";
    std::cout << "  Avg time: " << metric.avg_time.count() << " μs\n";
}
```

**Common Slow Query Causes**:
- Missing database indexes
- Connection pool exhaustion
- Large result sets without pagination
- N+1 query problem (use JOINs or batch loading)
- Network latency

### Q: Where can I find more help?

**A:** Resources:

- **Documentation**: [Full documentation](../../README.md)
- **Examples**: [samples/](../../samples/)
- **Architecture**: [docs/ARCHITECTURE.md](../ARCHITECTURE.md)
- **API Reference**: [docs/API_REFERENCE.md](../API_REFERENCE.md)
- **Issues**: [GitHub Issues](https://github.com/kcenon/database_system/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kcenon/database_system/discussions)

---

## Integration

### Q: How do I integrate database_system with other KCENON systems?

**A:** database_system integrates with the KCENON ecosystem:

**common_system** (Result<T> error handling):
```cpp
#include <kcenon/database/config/feature_flags.h>

#if KCENON_HAS_COMMON_SYSTEM
    #include <kcenon/common/patterns/result.h>
    using Result = kcenon::common::Result;
#else
    #include <database/core/result.h>
    using database::Result;
#endif

// Use Result<T> for error handling
auto result = db->connect("host=localhost");
if (!result.is_ok()) {
    std::cerr << "Error: " << result.error().message << "\n";
}
```

**thread_system** (High-performance async operations):
```cmake
# CMakeLists.txt
option(USE_THREAD_SYSTEM "Use thread_system for async operations" ON)
```

```cpp
// Async query with thread_system (1.16M ops/s, 77ns latency)
auto future = db->execute_async("SELECT * FROM large_table");
// Do other work...
auto result = future.get();
```

**logger_system** (Integrated logging):
```cpp
// Automatic logging integration
auto db = unified_database_system::create_builder()
    .enable_logging(db_log_level::debug, "./logs")
    .build();

// All database operations are automatically logged
db->execute("SELECT * FROM users");
// Log: [DEBUG] Executing query: SELECT * FROM users
```

**monitoring_system** (Performance metrics):
```cpp
// Export metrics to Prometheus
#include <database/monitoring/performance_monitor.h>

auto perf_monitor = context->get_performance_monitor();
prometheus_exporter exporter("http://prometheus:9090", 9091);

auto summary = perf_monitor->get_performance_summary();
exporter.export_metrics(summary);
```

See [Integration Guide](../INTEGRATION.md) for detailed examples.

### Q: Can I use database_system without other KCENON systems?

**A:** Yes! database_system has zero required dependencies:

**Standalone usage** (fallback implementations):
```bash
# Build without any dependencies
cmake .. \
  -DBUILD_WITH_COMMON_SYSTEM=OFF \
  -DUSE_LOGGER_SYSTEM=OFF \
  -DUSE_MONITORING_SYSTEM=OFF \
  -DUSE_THREAD_SYSTEM=OFF
```

**Fallback features**:
- Result<T>: Built-in implementation (no common_system)
- Logging: std::cout + file logging (no logger_system)
- Monitoring: Built-in metrics storage (no monitoring_system)
- Threading: std::thread pool (no thread_system)

**Example**:
```cpp
// Works without any dependencies
#include <database/integrated/unified_database_system.h>

auto db = unified_database_system::create_builder().build();
db->connect("host=localhost dbname=mydb");
auto result = db->execute("SELECT * FROM users");
```

### Q: How do I use database_system with container_system?

**A:** Store serialized containers in database BLOBs:

```cpp
#include <kcenon/container/value_container.h>
#include <database/database_manager.h>

using namespace kcenon;

// Serialize container to BLOB
value_container data;
data.set_value("name", "John");
data.set_value("age", 30);
data.set_value("email", "john@example.com");

auto serialized = data.serialize_to_bytes();

// Store in database
db->execute_query(
    "INSERT INTO user_data (user_id, data) VALUES ($1, $2)",
    {user_id, serialized}
);

// Retrieve from database
auto result = db->select_query("SELECT data FROM user_data WHERE user_id = $1", {user_id});
if (!result.empty()) {
    auto blob = result[0].at("data");
    value_container restored;
    restored.deserialize_from_bytes(blob);

    std::cout << "Name: " << restored.get_value<std::string>("name") << "\n";
    std::cout << "Age: " << restored.get_value<int>("age") << "\n";
}
```

---

## Advanced Topics

### Q: How does the backend plugin architecture work internally?

**A:** The backend plugin system uses runtime polymorphism:

**Architecture**:
```
backend_registry (Singleton)
    │
    ├── backend_factory (Factory Pattern)
    │   ├── "postgresql" → postgresql_backend
    │   ├── "mysql" → mysql_backend
    │   ├── "sqlite" → sqlite_backend
    │   ├── "mongodb" → mongodb_backend
    │   └── "redis" → redis_backend
    │
    └── database_backend (Abstract Interface)
        ├── connect(connection_config)
        ├── execute_query(query)
        ├── begin_transaction()
        ├── commit()
        └── rollback()
```

**Implementation**:
```cpp
// Backend interface
class database_backend {
public:
    virtual Result<bool> connect(const connection_config& config) = 0;
    virtual Result<query_result> execute_query(const std::string& query) = 0;
    virtual Result<bool> begin_transaction() = 0;
    virtual Result<bool> commit() = 0;
    virtual Result<bool> rollback() = 0;
};

// PostgreSQL implementation
class postgresql_backend : public database_backend {
private:
    std::unique_ptr<postgres_manager> manager_;

public:
    Result<bool> connect(const connection_config& config) override {
        std::string conn_str = build_connection_string(config);
        bool success = manager_->connect(conn_str);
        return success ? Result<bool>::ok(true)
                      : Result<bool>::error({-500, "Connection failed"});
    }

    // ... other methods
};

// Auto-registration
#ifdef USE_POSTGRESQL
namespace {
    backend_registrar<postgresql_backend> postgresql_registrar("postgresql");
}
#endif
```

For detailed implementation, see:
- [Backend Registry](../../database/core/backend_registry.h)
- [Database Backend Interface](../../database/core/database_backend.h)
- [Architecture Guide](../ARCHITECTURE.md)

### Q: Can I extend database_system with custom backends?

**A:** Yes! Implement the `database_backend` interface:

```cpp
#include <database/core/database_backend.h>
#include <database/core/backend_registry.h>

// Custom backend implementation
class custom_backend : public database_backend {
public:
    Result<bool> connect(const connection_config& config) override {
        // Implement custom connection logic
        return Result<bool>::ok(true);
    }

    Result<query_result> execute_query(const std::string& query) override {
        // Implement custom query execution
        query_result result;
        // ... populate result
        return Result<query_result>::ok(result);
    }

    Result<bool> begin_transaction() override {
        // Implement transaction support
        return Result<bool>::ok(true);
    }

    Result<bool> commit() override {
        return Result<bool>::ok(true);
    }

    Result<bool> rollback() override {
        return Result<bool>::ok(true);
    }
};

// Register custom backend
auto& registry = backend_registry::instance();
registry.register_backend("custom", []() -> std::unique_ptr<database_backend> {
    return std::make_unique<custom_backend>();
});

// Use custom backend
auto backend = registry.create("custom");
```

### Q: How do C++20 coroutines work in async operations?

**A:** Async operations use coroutines (C++20) or std::future (C++17):

**C++20 Coroutines** (requires C++20):
```cpp
#include <database/async/async_operations.h>

// Coroutine-based async query
database_awaitable<query_result> fetch_users_async() {
    auto result = co_await db.execute_async("SELECT * FROM users");
    co_return result;
}

// Usage
auto users = fetch_users_async().get();
```

**C++17 Fallback** (std::future):
```cpp
// Future-based async query (works in C++17)
auto future = db->execute_async("SELECT * FROM users");

// Do other work while query executes
process_other_data();

// Wait for result
auto result = future.get();
if (result.is_ok()) {
    for (const auto& row : result.value()) {
        // Process rows
    }
}
```

**Build Configuration**:
```cmake
# Coroutines auto-detected
# Falls back to std::future if C++20 unavailable
set(CMAKE_CXX_STANDARD 17)  # or 20 for coroutines
```

### Q: What is the dependency injection pattern used in database_system?

**A:** database_system uses constructor-based dependency injection:

**Legacy (Singleton)**:
```cpp
// ❌ Old way - singleton pattern
auto& db_mgr = database_manager::handle();
auto& pool_mgr = connection_pool_manager::instance();
auto& perf_mon = performance_monitor::instance();
```

**Modern (Dependency Injection)**:
```cpp
// ✅ New way - dependency injection
auto context = std::make_shared<database_context>();
auto db_mgr = std::make_shared<database_manager>(context);
auto pool_mgr = context->get_pool_manager();
auto perf_mon = context->get_performance_monitor();
```

**Benefits**:
- Unit testing with mock objects
- Multiple independent database instances
- Thread-safe by design (no global state)
- Better separation of concerns

See [IMPROVEMENT_PLAN.md](../../IMPROVEMENT_PLAN.md) for migration guide.

---

## Platform-Specific

### Q: Are there platform-specific considerations?

**A:**

**macOS**:
- Use Homebrew for dependencies: `brew install libpqxx postgresql sqlite`
- OpenSSL location: `/usr/local/opt/openssl` or `/opt/homebrew/opt/openssl`
- Clang is the default compiler (supports C++17/C++20)

**Linux (Ubuntu/Debian)**:
- Install dependencies: `sudo apt install libpqxx-dev libsqlite3-dev`
- GCC 7+ required for C++17, GCC 10+ for C++20
- PostgreSQL server: `sudo apt install postgresql`

**Windows**:
- Use vcpkg for dependencies (recommended)
- Visual Studio 2017+ required for C++17
- Visual Studio 2019+ required for C++20
- vcpkg integration: `cmake -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`

### Q: Does database_system support cross-compilation?

**A:** Yes, with these considerations:

**Supported target platforms**:
- ARM64 (Raspberry Pi, Apple Silicon)
- x86_64 (Intel/AMD)
- ARMv7 (older embedded systems)

**Cross-compilation example**:
```bash
# ARM64 cross-compilation
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=arm64-toolchain.cmake \
  -DUSE_POSTGRESQL=OFF \
  -DUSE_SQLITE=ON

# Embedded systems (minimal build)
cmake .. \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DUSE_POSTGRESQL=OFF \
  -DUSE_MYSQL=OFF \
  -DUSE_SQLITE=ON \
  -DBUILD_DATABASE_SAMPLES=OFF
```

**Known limitations**:
- MongoDB C++ driver may have limited ARM support
- Some database client libraries require native compilation
- Performance benchmarks may vary by architecture

---

## Contributing

### Q: How can I contribute to database_system?

**A:** We welcome contributions! See [Contributing Guide](../../CONTRIBUTING.md) for:

- Code style guidelines (C++17/C++20)
- Testing requirements (unit tests, integration tests)
- Pull request process
- Documentation standards

**Contribution areas**:
- New database backends
- Performance optimizations
- Documentation improvements
- Bug fixes
- Test coverage

### Q: I found a bug. Where should I report it?

**A:** Please report bugs on [GitHub Issues](https://github.com/kcenon/database_system/issues) with:

1. database_system version
2. Compiler and platform (GCC 7, Ubuntu 20.04, etc.)
3. Database backend (PostgreSQL 13, MySQL 8, etc.)
4. Minimal reproduction case
5. Expected vs actual behavior

**Example bug report**:
```
Title: Connection pool deadlock with MySQL backend

Environment:
- database_system: v1.0
- Compiler: GCC 10.2, Ubuntu 20.04
- Database: MySQL 8.0

Reproduction:
1. Create connection pool with max_connections=5
2. Spawn 10 threads acquiring connections
3. All threads hang after 5 connections acquired

Expected: Timeout after acquire_timeout
Actual: Deadlock, no timeout

Code:
[minimal reproduction code]
```

---

## Related Documentation

- [README](../../README.md) - Project overview and features
- [Architecture](../ARCHITECTURE.md) - System architecture and design patterns
- [Build Guide](../BUILD_GUIDE.md) - Comprehensive build instructions
- [API Reference](../API_REFERENCE.md) - Complete API documentation
- [Troubleshooting](TROUBLESHOOTING.md) - Detailed troubleshooting guide
- [Integration](../INTEGRATION.md) - Integration with KCENON ecosystem
- [Performance Benchmarks](../PERFORMANCE_BENCHMARKS.md) - Detailed performance metrics
- [Improvement Plan](../../IMPROVEMENT_PLAN.md) - Roadmap and recent improvements

---

## Document Maintenance

This FAQ is updated regularly. If you have a question not answered here:

1. Check [full documentation](../../README.md)
2. Search [closed issues](https://github.com/kcenon/database_system/issues?q=is%3Aissue+is%3Aclosed)
3. Ask on [GitHub Discussions](https://github.com/kcenon/database_system/discussions)
4. Submit a documentation improvement [issue](https://github.com/kcenon/database_system/issues/new)

---

**Last Review**: 2025-11-11
**Next Review**: 2026-02-11 (Quarterly review schedule)
**Maintainer**: Database System Team (@kcenon)
