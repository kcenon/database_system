---
doc_id: "DBS-FEAT-002"
doc_title: "Database System Features"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "FEAT"
---

# Database System Features

**Last Updated**: 2026-02-08
**Version**: 0.4.0.0

This document provides comprehensive details on all database_system features, backend implementations, and capabilities.

---

## Table of Contents

- [Multi-Backend Support](#multi-backend-support)
- [ORM Framework](#orm-framework)
- [Connection Pooling](#connection-pooling)
- [Query Builders](#query-builders)
- [Resilient Connections](#resilient-connections)
- [Enterprise Security](#enterprise-security)
- [Performance Monitoring](#performance-monitoring)
- [Asynchronous Operations](#asynchronous-operations)
- [Proxy Mode](#proxy-mode)
- [Unified Database System](#unified-database-system)
- [common_system Integration](#common_system-integration)
- [C++20 Modules](#c20-modules)

---

## Multi-Backend Support

The database_system provides unified access to multiple database backends with consistent API.

### PostgreSQL Backend

**Status**: ✅ Full Support
**Implementation**: `postgres_manager.h/cpp`

**Features**:
- JSONB data type support with advanced queries
- Array types with efficient storage
- Common Table Expressions (CTEs) for complex queries
- Prepared statements with parameter binding
- Full-text search with tsvector
- Concurrent index builds
- Advanced window functions
- Materialized views
- Row-level security

**Advanced Capabilities**:
```cpp
// JSONB operations
auto result = db.create_query_builder(database_types::postgres)
    .select({"id", "data->>'name' as name", "data->'address'->>'city' as city"})
    .from("users")
    .where("data @> '{\"active\": true}'::jsonb")
    .execute(&db);

// Array operations
auto array_result = db.create_query_builder(database_types::postgres)
    .insert_into("tags")
    .values({
        {"name", database_value{std::string("product1")}},
        {"tags", database_value{std::string("ARRAY['electronics', 'gadgets', 'new']")}}
    })
    .execute(&db);

// CTEs for complex queries
auto cte_result = db.create_query_builder(database_types::postgres)
    .raw_sql(R"(
        WITH sales_summary AS (
            SELECT
                product_id,
                SUM(amount) as total_sales,
                COUNT(*) as order_count
            FROM orders
            WHERE created_at > NOW() - INTERVAL '30 days'
            GROUP BY product_id
        )
        SELECT p.name, s.total_sales, s.order_count
        FROM products p
        JOIN sales_summary s ON p.id = s.product_id
        ORDER BY s.total_sales DESC
        LIMIT 10
    )")
    .execute(&db);
```

**Configuration Options**:
- Connection string: `host=localhost port=5432 dbname=mydb user=admin password=secret sslmode=require`
- SSL/TLS encryption with certificate verification
- Connection timeout and keepalive settings
- Statement timeout configuration
- Work memory and shared buffers tuning

### SQLite Backend

**Status**: ✅ Full Support
**Implementation**: `sqlite/sqlite_manager.h/cpp`

**Features**:
- WAL (Write-Ahead Logging) mode for concurrency
- FTS5 full-text search engine
- In-memory databases for testing
- Embedded database (no server required)
- JSON1 extension support
- Common Table Expressions
- Window functions
- Partial indexes
- Generated columns

**Advanced Capabilities**:
```cpp
// WAL mode configuration
db.execute_command("PRAGMA journal_mode=WAL");
db.execute_command("PRAGMA synchronous=NORMAL");

// FTS5 full-text search
db.execute_command(R"(
    CREATE VIRTUAL TABLE documents_fts USING fts5(
        title, content, tags,
        tokenize = 'porter ascii'
    )
)");

auto fts_result = db.create_query_builder(database_types::sqlite)
    .select({"title", "highlight(documents_fts, 1, '<b>', '</b>') as snippet"})
    .from("documents_fts")
    .where("documents_fts MATCH 'database AND performance'")
    .execute(&db);

// In-memory database
auto mem_db = std::make_shared<sqlite_manager>();
mem_db->connect(":memory:");

// JSON operations (JSON1 extension)
auto json_result = db.create_query_builder(database_types::sqlite)
    .select({"id", "json_extract(data, '$.name') as name", "json_extract(data, '$.age') as age"})
    .from("users")
    .where("json_extract(data, '$.active') = 1")
    .execute(&db);
```

**Configuration Options**:
- File path or `:memory:` for in-memory
- WAL mode for concurrent reads
- Synchronous mode (FULL, NORMAL, OFF)
- Cache size and page size
- Busy timeout for lock contention
- Foreign keys enforcement

### MongoDB Backend

**Status**: 🧪 Experimental (disabled by default)
**Implementation**: `mongodb/mongodb_manager.h/cpp`

> ⚠️ **Experimental**: MongoDB support is functional but experimental. Enable with `USE_MONGODB=ON` CMake option.

**Features**:
- Document-based storage with BSON
- Aggregation pipeline framework
- GridFS for large file storage
- Sharding and replication support
- Text search indexes
- Geospatial queries
- Change streams for real-time updates
- Transactions (MongoDB 4.0+)
- Time series collections (MongoDB 5.0+)

**Advanced Capabilities**:
```cpp
// Aggregation pipeline
auto agg_result = db.create_query_builder(database_types::mongodb)
    .collection("orders")
    .aggregate({
        {"$match", {{"status", database_value{std::string("completed")}}}},
        {"$group", {
            {"_id", database_value{std::string("$customer_id")}},
            {"total_amount", {{"$sum", database_value{std::string("$amount")}}}},
            {"order_count", {{"$sum", database_value{int64_t(1)}}}}
        }},
        {"$sort", {{"total_amount", database_value{int64_t(-1)}}}},
        {"$limit", database_value{int64_t(10)}}
    })
    .execute(&db);

// GridFS for file storage
auto gridfs = db.get_gridfs_bucket("uploads");
auto file_id = gridfs->upload_file("document.pdf", file_data);
auto file_data_retrieved = gridfs->download_file(file_id);

// Text search
db.execute_command(R"({"createIndexes": "articles", "indexes": [{"key": {"title": "text", "content": "text"}, "name": "text_idx"}]})");
auto search_result = db.create_query_builder(database_types::mongodb)
    .collection("articles")
    .find({{"$text", {{"$search", database_value{std::string("database performance")}}}}})
    .execute(&db);

// Change streams
auto stream = db.watch_collection("users");
stream->on_change([](const change_event& event) {
    std::cout << "Change detected: " << event.operation_type << std::endl;
});
```

**Configuration Options**:
- Connection string: `mongodb://localhost:27017/mydb?replicaSet=rs0&authSource=admin`
- Replica set configuration
- Read preference (primary, secondary, nearest)
- Write concern levels
- Read concern levels
- Connection pool size
- Server selection timeout

### Redis Backend

**Status**: 🧪 Experimental (disabled by default)
**Implementation**: `redis/redis_manager.h/cpp`

> ⚠️ **Experimental**: Redis support is functional but experimental. Enable with `USE_REDIS=ON` CMake option.

**Features**:
- All data types (Strings, Hashes, Lists, Sets, Sorted Sets)
- Pub/Sub messaging pattern
- Transactions with MULTI/EXEC
- Lua scripting support
- Pipelining for batch operations
- Persistence (RDB and AOF)
- Cluster mode support
- Streams for event sourcing
- Geospatial indexes

**Advanced Capabilities**:
```cpp
// Hash operations
auto hash_result = db.create_query_builder(database_types::redis)
    .hset("user:1000", {
        {"username", "john_doe"},
        {"email", "john@example.com"},
        {"last_login", std::to_string(std::time(nullptr))}
    })
    .execute(&db);

// Sorted set with scores
db.execute_command("ZADD leaderboard 1000 player1 950 player2 1200 player3");
auto leaderboard = db.create_query_builder(database_types::redis)
    .zrevrange("leaderboard", 0, 9, true)  // Top 10 with scores
    .execute(&db);

// Pub/Sub messaging
auto subscriber = db.create_subscriber();
subscriber->subscribe("notifications", [](const std::string& channel, const std::string& message) {
    std::cout << "Received on " << channel << ": " << message << std::endl;
});

auto publisher = db.create_publisher();
publisher->publish("notifications", "New message available");

// Lua scripting
std::string lua_script = R"(
    local current = redis.call('GET', KEYS[1])
    if not current then current = '0' end
    local new_val = tonumber(current) + tonumber(ARGV[1])
    redis.call('SET', KEYS[1], tostring(new_val))
    return new_val
)";
auto script_result = db.eval_script(lua_script, {"counter:total"}, {"10"});

// Redis Streams
db.execute_command("XADD events * action purchase product_id 12345 amount 99.99");
auto stream_result = db.create_query_builder(database_types::redis)
    .xread("events", "0-0", 100)  // Read 100 messages from beginning
    .execute(&db);
```

**Configuration Options**:
- Connection string: `redis://localhost:6379/0` or `redis://user:password@host:6379/db`
- SSL/TLS support: `rediss://host:6380`
- Cluster mode: `redis://node1:6379,node2:6379,node3:6379`
- Connection pool size
- Timeout settings
- Retry strategy
- Sentinel configuration for high availability

---

## ORM Framework

**Status**: ✅ Full Support (C++17 SFINAE-based)
**Implementation**: `orm/entity.h`, `orm/entity_manager.h`, `orm/schema_manager.h`

### Entity Definition

Define database entities using C++17 SFINAE-based macros:

```cpp
#include <database/orm/entity.h>

class User : public entity_base {
    ENTITY_TABLE("users")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique() | index("idx_username"))
    ENTITY_FIELD(std::string, email, not_null() | unique())
    ENTITY_FIELD(std::string, password_hash, not_null())
    ENTITY_FIELD(std::chrono::system_clock::time_point, created_at, default_now())
    ENTITY_FIELD(std::chrono::system_clock::time_point, updated_at, on_update_now())
    ENTITY_FIELD(bool, is_active, default_value(true))
    ENTITY_FIELD(std::optional<std::string>, profile_image)

    ENTITY_METADATA()
};

class Post : public entity_base {
    ENTITY_TABLE("posts")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(int64_t, user_id, foreign_key("users", "id") | not_null())
    ENTITY_FIELD(std::string, title, not_null() | index("idx_title"))
    ENTITY_FIELD(std::string, content, not_null())
    ENTITY_FIELD(std::vector<std::string>, tags, default_value(std::vector<std::string>{}))
    ENTITY_FIELD(std::chrono::system_clock::time_point, published_at, default_now())

    ENTITY_METADATA()
};
```

### Entity Operations

**Create (Insert)**:
```cpp
User user;
user.username = "john_doe";
user.email = "john@example.com";
user.password_hash = hash_password("secure_password");

auto create_result = user.save(db);
if (create_result) {
    std::cout << "User created with ID: " << user.id << std::endl;
}
```

**Read (Query)**:
```cpp
// Find by ID
auto user_result = User::find(db, 12345);
if (user_result) {
    std::cout << "Username: " << user_result->username << std::endl;
}

// Query with conditions
auto active_users = User::query(db)
    .where("is_active = ?", true)
    .where("created_at > ?", one_week_ago)
    .order_by("username")
    .limit(100)
    .execute();

for (const auto& user : active_users) {
    std::cout << user.username << " - " << user.email << std::endl;
}

// Complex queries
auto popular_posts = Post::query(db)
    .join("users", "posts.user_id = users.id")
    .where("posts.published_at > ?", one_month_ago)
    .group_by("posts.id")
    .having("COUNT(comments.id) > ?", 10)
    .order_by("COUNT(comments.id)", sort_order::desc)
    .limit(20)
    .execute();
```

**Update**:
```cpp
auto user = User::find(db, 12345);
if (user) {
    user->email = "newemail@example.com";
    user->updated_at = std::chrono::system_clock::now();

    auto update_result = user->save(db);
    if (update_result) {
        std::cout << "User updated successfully" << std::endl;
    }
}

// Bulk update
auto update_count = User::query(db)
    .where("last_login < ?", one_year_ago)
    .update({{"is_active", database_value{false}}});
std::cout << "Deactivated " << update_count << " inactive users" << std::endl;
```

**Delete**:
```cpp
auto user = User::find(db, 12345);
if (user) {
    auto delete_result = user->remove(db);
    if (delete_result) {
        std::cout << "User deleted successfully" << std::endl;
    }
}

// Bulk delete
auto delete_count = Post::query(db)
    .where("published_at < ?", two_years_ago)
    .remove();
std::cout << "Deleted " << delete_count << " old posts" << std::endl;
```

### Relationships

**One-to-Many**:
```cpp
class User : public entity_base {
    ENTITY_TABLE("users")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null())

    ENTITY_RELATIONSHIP(has_many, Post, "user_id")
    ENTITY_METADATA()
};

// Access related posts
auto user = User::find(db, 12345);
auto posts = user->posts(db);  // Lazy-loaded
for (const auto& post : posts) {
    std::cout << post.title << std::endl;
}
```

**Many-to-One**:
```cpp
class Post : public entity_base {
    ENTITY_TABLE("posts")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(int64_t, user_id, foreign_key("users", "id"))

    ENTITY_RELATIONSHIP(belongs_to, User, "user_id")
    ENTITY_METADATA()
};

// Access related user
auto post = Post::find(db, 678);
auto author = post->user(db);  // Lazy-loaded
std::cout << "Author: " << author.username << std::endl;
```

**Many-to-Many**:
```cpp
class Tag : public entity_base {
    ENTITY_TABLE("tags")
    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, name, not_null() | unique())

    ENTITY_RELATIONSHIP(many_to_many, Post, "post_tags", "tag_id", "post_id")
    ENTITY_METADATA()
};

// Access related posts through junction table
auto tag = Tag::find(db, 5);
auto tagged_posts = tag->posts(db);
```

### Schema Management

**Automatic Schema Generation**:
```cpp
#include <database/orm/schema_manager.h>

auto& schema_mgr = schema_manager::instance();

// Generate CREATE TABLE statements
auto create_sql = schema_mgr.generate_schema<User>();
std::cout << create_sql << std::endl;
// Output:
// CREATE TABLE users (
//     id BIGSERIAL PRIMARY KEY,
//     username VARCHAR(255) NOT NULL UNIQUE,
//     email VARCHAR(255) NOT NULL UNIQUE,
//     password_hash VARCHAR(255) NOT NULL,
//     created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
//     updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
//     is_active BOOLEAN DEFAULT TRUE,
//     profile_image VARCHAR(255)
// );
// CREATE INDEX idx_username ON users(username);

// Create all tables
schema_mgr.create_tables(db, {
    schema_mgr.generate_schema<User>(),
    schema_mgr.generate_schema<Post>(),
    schema_mgr.generate_schema<Tag>()
});
```

**Schema Migrations**:
```cpp
// Define migration
migration migration_001("add_user_bio", [](database_manager& db) {
    return db.execute_command(
        "ALTER TABLE users ADD COLUMN bio TEXT"
    );
});

// Register and run migrations
schema_mgr.register_migration(migration_001);
schema_mgr.run_migrations(db);

// Migration history tracking
auto applied_migrations = schema_mgr.get_applied_migrations(db);
for (const auto& migration_name : applied_migrations) {
    std::cout << "Applied: " << migration_name << std::endl;
}
```

### Advanced ORM Features

**Eager Loading** (N+1 query prevention):
```cpp
auto users_with_posts = User::query(db)
    .with("posts")  // Eager load posts
    .with("posts.comments")  // Eager load comments through posts
    .execute();

// All data loaded in optimized queries (3 queries instead of N+1)
for (const auto& user : users_with_posts) {
    std::cout << user.username << " has " << user.posts().size() << " posts" << std::endl;
    for (const auto& post : user.posts()) {
        std::cout << "  - " << post.title << " (" << post.comments().size() << " comments)" << std::endl;
    }
}
```

**Scopes** (Reusable query filters):
```cpp
class Post : public entity_base {
    // ... fields ...

    ENTITY_SCOPE(published, [](query_builder& q) {
        return q.where("published_at IS NOT NULL")
                .where("published_at <= ?", std::chrono::system_clock::now());
    })

    ENTITY_SCOPE(popular, [](query_builder& q, int min_views = 1000) {
        return q.where("view_count >= ?", min_views);
    })

    ENTITY_METADATA()
};

// Use scopes
auto popular_published = Post::query(db)
    .published()
    .popular(5000)
    .order_by("view_count", sort_order::desc)
    .execute();
```

**Soft Deletes**:
```cpp
class User : public entity_base {
    ENTITY_TABLE("users")
    // ... other fields ...
    ENTITY_FIELD(std::optional<std::chrono::system_clock::time_point>, deleted_at)

    ENTITY_SOFT_DELETE("deleted_at")
    ENTITY_METADATA()
};

// Soft delete (sets deleted_at instead of removing row)
user->remove(db);  // Sets deleted_at = NOW()

// Query only non-deleted
auto active_users = User::query(db)
    .execute();  // Automatically filters WHERE deleted_at IS NULL

// Query including deleted
auto all_users = User::query(db)
    .with_trashed()
    .execute();

// Query only deleted
auto deleted_users = User::query(db)
    .only_trashed()
    .execute();

// Restore soft-deleted
user->restore(db);  // Sets deleted_at = NULL
```

**Observers** (Lifecycle hooks):
```cpp
class User : public entity_base {
    // ... fields ...

    ENTITY_OBSERVER(before_create, [](User& user) {
        user.created_at = std::chrono::system_clock::now();
        user.updated_at = user.created_at;
    })

    ENTITY_OBSERVER(before_update, [](User& user) {
        user.updated_at = std::chrono::system_clock::now();
    })

    ENTITY_OBSERVER(after_delete, [](const User& user) {
        // Log deletion
        audit_log("User deleted: " + user.username);
    })

    ENTITY_METADATA()
};
```

---

## Connection Pooling

**Status**: ✅ Well-Tested (v3)
**Implementation**: `connection_pool.h/cpp`

### Connection Pool v3 Features

**Performance Improvements**:
- **77ns latency**: 65x faster than v2 (5μs → 77ns)
- **1.16M+ ops/s**: Throughput with thread_system integration
- **7.7x performance**: Under high load with adaptive job queue
- **Priority scheduling**: Connection acquisition with QoS levels
- **Graceful shutdown**: Cancellation tokens for clean termination

**Architecture**:
```cpp
#include <database/connection_pool.h>

// Connection pool configuration
connection_pool_config config;
config.min_connections = 10;           // Minimum pool size
config.max_connections = 100;          // Maximum pool size
config.acquire_timeout = std::chrono::seconds(5);
config.idle_timeout = std::chrono::seconds(30);
config.health_check_interval = std::chrono::seconds(60);
config.enable_health_checks = true;
config.connection_string = "host=localhost port=5432 dbname=mydb";

// Create connection pool
auto& db = database_manager::handle();
db.create_connection_pool(database_types::postgres, config);

// Acquire connection (RAII-managed)
auto pool = db.get_connection_pool(database_types::postgres);
auto connection = pool->acquire_connection();

if (connection) {
    // Use connection for operations
    auto result = connection->select_query("SELECT * FROM users");

    // Connection automatically returned to pool when destroyed
}
```

### Priority-Based Acquisition

```cpp
// High-priority connection for critical operations
auto critical_conn = pool->acquire_connection(connection_priority::high);

// Normal priority (default)
auto normal_conn = pool->acquire_connection(connection_priority::normal);

// Low priority for background tasks
auto background_conn = pool->acquire_connection(connection_priority::low);
```

### Health Monitoring

```cpp
// Enable automatic health checks
config.enable_health_checks = true;
config.health_check_interval = std::chrono::seconds(30);

// Health check query
config.health_check_query = "SELECT 1";

// Get pool statistics
auto stats = pool->get_statistics();
std::cout << "Active connections: " << stats.active_connections << std::endl;
std::cout << "Available connections: " << stats.available_connections << std::endl;
std::cout << "Total created: " << stats.total_created << std::endl;
std::cout << "Total destroyed: " << stats.total_destroyed << std::endl;
std::cout << "Failed health checks: " << stats.failed_health_checks << std::endl;
std::cout << "Average acquisition time: " << stats.avg_acquisition_time.count() << "ns" << std::endl;

// Check pool health
if (pool->is_healthy()) {
    std::cout << "Pool is healthy" << std::endl;
}
```

### Thread-System Integration

```cpp
#include <thread_system/thread_pool.h>

// Use thread_system for async connection acquisition
auto thread_pool = std::make_shared<thread_system::thread_pool>(8);

// Submit connection acquisition task
auto future = thread_pool->submit([&pool]() {
    auto conn = pool->acquire_connection();
    if (conn) {
        return conn->select_query("SELECT * FROM large_table");
    }
    return database_result{};
});

// Do other work...

// Get result when ready
auto result = future.get();
```

### Graceful Shutdown

```cpp
// Create cancellation token
auto shutdown_token = std::make_shared<cancellation_token>();

// Register signal handler
std::signal(SIGTERM, [](int) {
    shutdown_token->cancel();
});

// Pool respects cancellation
pool->set_cancellation_token(shutdown_token);

// On shutdown, pool drains gracefully
pool->shutdown();  // Waits for active connections, rejects new requests
```

---

## Query Builders

**Status**: ✅ Full Support
**Implementation**: `query/query_builder.h`, `query/sql_builder.h`, `query/nosql_builder.h`

### Immutable Query Builder

**New in v3**: Thread-safe query construction with functional programming style:

```cpp
#include <database/query/immutable_query_builder.h>

// Immutable builder (each method returns new instance)
const auto base_query = immutable_query_builder()
    .select({"id", "name", "email"})
    .from("users");

// Branch 1: Active users
const auto active_users = base_query
    .where("is_active", "=", database_value{true})
    .order_by("name");

// Branch 2: Admin users (base_query unchanged)
const auto admin_users = base_query
    .where("role", "=", database_value{std::string("admin")})
    .order_by("created_at", sort_order::desc);

// Thread-safe: No race conditions
std::thread t1([&]() {
    auto result1 = active_users.execute(&db);
});

std::thread t2([&]() {
    auto result2 = admin_users.execute(&db);
});

t1.join();
t2.join();
```

### SQL Query Builder

**Comprehensive SQL Support**:

```cpp
#include <database/query/sql_builder.h>

// SELECT with complex conditions
auto query = db.create_query_builder(database_types::postgres)
    .select({"u.id", "u.username", "COUNT(p.id) as post_count"})
    .from("users u")
    .join("posts p", "u.id = p.user_id", join_type::left)
    .where("u.is_active", "=", database_value{true})
    .where("u.created_at", ">", database_value{one_month_ago})
    .group_by("u.id", "u.username")
    .having("COUNT(p.id)", ">", database_value{int64_t(5)})
    .order_by("post_count", sort_order::desc)
    .limit(20)
    .offset(0);

auto sql = query.build();
std::cout << "Generated SQL: " << sql << std::endl;

auto result = query.execute(&db);
```

**INSERT Operations**:
```cpp
// Single insert
auto insert_query = db.create_query_builder(database_types::postgres)
    .insert_into("users")
    .values({
        {"username", database_value{std::string("john_doe")}},
        {"email", database_value{std::string("john@example.com")}},
        {"age", database_value{int64_t(30)}},
        {"is_active", database_value{true}}
    });

auto insert_result = insert_query.execute(&db);

// Bulk insert
auto bulk_insert = db.create_query_builder(database_types::postgres)
    .insert_into("users")
    .values_bulk({
        {
            {"username", database_value{std::string("user1")}},
            {"email", database_value{std::string("user1@example.com")}}
        },
        {
            {"username", database_value{std::string("user2")}},
            {"email", database_value{std::string("user2@example.com")}}
        }
    });

auto bulk_result = bulk_insert.execute(&db);
```

**UPDATE Operations**:
```cpp
auto update_query = db.create_query_builder(database_types::postgres)
    .update("users")
    .set({
        {"last_login", database_value{std::to_string(std::time(nullptr))}},
        {"login_count", database_value{std::string("login_count + 1")}}
    })
    .where("id", "=", database_value{int64_t(12345)});

auto update_result = update_query.execute(&db);
```

**DELETE Operations**:
```cpp
auto delete_query = db.create_query_builder(database_types::postgres)
    .delete_from("sessions")
    .where("expires_at", "<", database_value{std::to_string(std::time(nullptr))});

auto delete_result = delete_query.execute(&db);
```

### NoSQL Query Builder

**MongoDB Query Builder**:

```cpp
#include <database/query/nosql_builder.h>

// Find documents
auto mongo_query = db.create_query_builder(database_types::mongodb)
    .collection("users")
    .find({
        {"age", {{"$gt", database_value{int64_t(18)}}}},
        {"status", database_value{std::string("active")}}
    })
    .sort("created_at", -1)
    .limit(100)
    .skip(0);

auto result = mongo_query.execute(&db);

// Aggregation
auto agg_query = db.create_query_builder(database_types::mongodb)
    .collection("orders")
    .aggregate({
        {"$match", {{"status", database_value{std::string("completed")}}}},
        {"$group", {
            {"_id", database_value{std::string("$customer_id")}},
            {"total", {{"$sum", database_value{std::string("$amount")}}}}
        }},
        {"$sort", {{"total", database_value{int64_t(-1)}}}},
        {"$limit", database_value{int64_t(10)}}
    });

// Insert document
auto insert_doc = db.create_query_builder(database_types::mongodb)
    .collection("users")
    .insert_one({
        {"username", database_value{std::string("john_doe")}},
        {"email", database_value{std::string("john@example.com")}},
        {"age", database_value{int64_t(30)}},
        {"tags", database_value{std::vector<std::string>{"developer", "blogger"}}}
    });

// Update document
auto update_doc = db.create_query_builder(database_types::mongodb)
    .collection("users")
    .update_one(
        {{"username", database_value{std::string("john_doe")}}},
        {{"$set", {{"last_login", database_value{std::to_string(std::time(nullptr))}}}}}
    );
```

**Redis Query Builder**:

```cpp
// String operations
auto redis_set = db.create_query_builder(database_types::redis)
    .set("user:1000:name", "John Doe")
    .execute(&db);

auto redis_get = db.create_query_builder(database_types::redis)
    .get("user:1000:name")
    .execute(&db);

// Hash operations
auto redis_hset = db.create_query_builder(database_types::redis)
    .hset("user:1000", {
        {"username", "john_doe"},
        {"email", "john@example.com"},
        {"age", "30"}
    })
    .execute(&db);

auto redis_hgetall = db.create_query_builder(database_types::redis)
    .hgetall("user:1000")
    .execute(&db);

// List operations
auto redis_lpush = db.create_query_builder(database_types::redis)
    .lpush("notifications", {"New message", "Friend request"})
    .execute(&db);

// Set operations
auto redis_sadd = db.create_query_builder(database_types::redis)
    .sadd("tags:popular", {"database", "performance", "c++"})
    .execute(&db);

// Sorted set operations
auto redis_zadd = db.create_query_builder(database_types::redis)
    .zadd("leaderboard", {
        {1000, "player1"},
        {950, "player2"},
        {1200, "player3"}
    })
    .execute(&db);
```

---

## Resilient Connections

**Status**: ✅ Well-Tested
**Implementation**: `resilient/resilient_connection.h`

### Automatic Reconnection

Production-grade reliability with exponential backoff:

```cpp
#include <database/resilient/resilient_connection.h>

// Configure resilient connection
resilient_connection_config resilient_config;
resilient_config.max_retries = 5;
resilient_config.initial_retry_delay = std::chrono::milliseconds(100);
resilient_config.max_retry_delay = std::chrono::seconds(30);
resilient_config.backoff_multiplier = 2.0;  // Exponential backoff
resilient_config.enable_jitter = true;  // Randomize retry delays
resilient_config.circuit_breaker_threshold = 10;  // Failures before circuit opens
resilient_config.circuit_breaker_timeout = std::chrono::seconds(60);

// Create resilient connection wrapper
auto base_connection = std::make_shared<postgres_manager>();
auto resilient_conn = std::make_shared<resilient_connection>(base_connection, resilient_config);

// Connect with automatic retry
auto connect_result = resilient_conn->connect("host=localhost port=5432 dbname=mydb");
// Automatically retries with exponential backoff on failure
// Recovery time: <1s for transient failures

// Queries automatically retry on connection loss
auto result = resilient_conn->select_query("SELECT * FROM users");
// If connection lost, automatically reconnects and retries query
```

### Health Monitoring

Real-time connection quality scoring:

```cpp
// Get connection health
auto health_score = resilient_conn->get_health_score();
std::cout << "Connection health: " << health_score << "/100" << std::endl;

// Health score based on:
// - Connection uptime
// - Query success rate
// - Response time
// - Error frequency

// Register health change callback
resilient_conn->on_health_change([](int old_score, int new_score) {
    std::cout << "Health changed: " << old_score << " -> " << new_score << std::endl;
    if (new_score < 50) {
        alert("Database connection degraded");
    }
});

// Get detailed health metrics
auto health_metrics = resilient_conn->get_health_metrics();
std::cout << "Uptime: " << health_metrics.uptime.count() << "s" << std::endl;
std::cout << "Total queries: " << health_metrics.total_queries << std::endl;
std::cout << "Failed queries: " << health_metrics.failed_queries << std::endl;
std::cout << "Success rate: " << (health_metrics.success_rate * 100) << "%" << std::endl;
std::cout << "Avg response time: " << health_metrics.avg_response_time.count() << "ms" << std::endl;
```

### Circuit Breaker Pattern

Prevents cascade failures:

```cpp
// Circuit breaker automatically opens after threshold failures
resilient_config.circuit_breaker_threshold = 5;
resilient_config.circuit_breaker_timeout = std::chrono::seconds(30);

// After 5 consecutive failures, circuit opens
// All requests immediately fail for 30 seconds
// Then circuit enters half-open state (tries one request)
// If successful, circuit closes; if failed, remains open

// Get circuit state
auto circuit_state = resilient_conn->get_circuit_state();
switch (circuit_state) {
    case circuit_state::closed:
        std::cout << "Circuit closed (normal operation)" << std::endl;
        break;
    case circuit_state::open:
        std::cout << "Circuit open (failing fast)" << std::endl;
        break;
    case circuit_state::half_open:
        std::cout << "Circuit half-open (testing recovery)" << std::endl;
        break;
}
```

---

## Enterprise Security

**Status**: ✅ Full Support
**Implementation**: `security/secure_connection.h`

The security module provides six dedicated components accessible via dependency injection through `database_context`.

### Credential Manager

Encrypted credential storage with master key management:

```cpp
#include <database/security/secure_connection.h>

auto context = std::make_shared<database_context>();
auto cred_mgr = context->get_credential_manager();

// Store encrypted credentials
security::security_credentials creds;
creds.username = "admin";
creds.password_hash = cred_mgr->hash_password("secure_pass");
creds.auth_method = security::authentication_method::password;
creds.encryption = security::encryption_type::tls;
cred_mgr->store_credentials("primary_db", creds);

// Retrieve credentials
auto stored = cred_mgr->get_credentials("primary_db");

// Key rotation
cred_mgr->set_master_key("new-master-key");
cred_mgr->rotate_encryption_keys();
```

**Supported Authentication Methods**:
- Password-based authentication
- Certificate-based authentication
- Kerberos
- OAuth2 (with client_id, client_secret, token management)
- JWT (with token expiry tracking)

### Connection Security

Secure database connections with TLS/SSL and mutual authentication:

```cpp
security::security_credentials creds;
creds.encryption = security::encryption_type::tls;
creds.verify_certificate = true;
creds.mutual_authentication = true;

security::connection_security conn_sec(creds);

// Configure TLS
conn_sec.configure_tls("client.crt", "client.key", "ca.crt");
conn_sec.set_cipher_suite("TLS_AES_256_GCM_SHA384");

// Establish secure connection
conn_sec.establish_secure_connection("db.example.com", 5432);

// Connection string encryption for secure storage
auto encrypted = conn_sec.encrypt_connection_string("host=localhost password=secret");
auto decrypted = conn_sec.decrypt_connection_string(encrypted);
```

### Query Security

SQL injection prevention and query analysis:

```cpp
// SQL injection detection
bool safe = security::query_security::is_query_safe(user_input);
std::string sanitized = security::query_security::sanitize_input(user_input);
std::string escaped = security::query_security::escape_sql_string(user_value);

// Suspicious pattern detection
bool suspicious = security::query_security::detect_suspicious_patterns(query);

// Table access validation
auto tables = security::query_security::extract_table_names(query);
bool allowed = security::query_security::validate_table_access("users", "SELECT", "admin");

// Convert to prepared statement
auto prepared = security::query_security::convert_to_prepared_statement(
    query, {database_value{std::string("param1")}, database_value{int64_t(42)}});
```

### Role-Based Access Control (RBAC)

Fine-grained permission management:

```cpp
auto access_ctrl = context->get_access_control();

// Create roles with permissions
security::access_control::role admin_role;
admin_role.name = "db_admin";
admin_role.permissions = {
    security::access_control::permission::select,
    security::access_control::permission::insert,
    security::access_control::permission::update,
    security::access_control::permission::admin
};
admin_role.allowed_tables = {"*"};
access_ctrl->create_role(admin_role);

// Assign roles and check permissions
access_ctrl->assign_role_to_user("user_123", "db_admin");
bool can_delete = access_ctrl->check_permission("user_123", "users", "DELETE");

// Session management
auto session_id = access_ctrl->create_session("user_123", "192.168.1.100");
bool valid = access_ctrl->validate_session(session_id);
access_ctrl->cleanup_expired_sessions();
```

### Security Audit Logger

Comprehensive security event logging and reporting:

```cpp
auto audit_log = context->get_audit_logger();

// Log database access events
audit_log->log_database_access("user_123", session_id, "SELECT", "users", query_hash, true);
audit_log->log_authentication_event("user_123", "192.168.1.100", true, "password");
audit_log->log_authorization_failure("user_456", "DROP", "users", "Insufficient privileges");

// Retrieve audit logs
auto recent_logs = audit_log->get_audit_logs(std::chrono::hours(24));
auto user_logs = audit_log->get_user_audit_logs("user_123", std::chrono::hours(168));

// Security reporting
auto report = audit_log->generate_security_report(std::chrono::hours(720));
auto suspicious = audit_log->detect_suspicious_activity(std::chrono::hours(24));

// Log management
audit_log->set_log_retention_period(std::chrono::hours(24 * 90));  // 90 days
audit_log->export_logs_to_file("audit_2026_Q1.log");
```

### Security Monitor

Real-time threat detection and alerting:

```cpp
auto sec_monitor = context->get_security_monitor();

// Register alert handler
sec_monitor->register_security_handler([](const security::security_monitor::security_alert& alert) {
    if (alert.level == security::security_monitor::threat_level::critical) {
        send_alert_notification(alert.description);
    }
});

// Active monitoring (called automatically by the system)
sec_monitor->analyze_query_patterns("user_123", query);
sec_monitor->detect_brute_force_attempts("192.168.1.100");
sec_monitor->monitor_privilege_escalation("user_456", "ALTER TABLE");

// Security metrics
auto failed_logins = sec_monitor->get_failed_login_count(std::chrono::hours(1));
auto suspicious_queries = sec_monitor->get_suspicious_query_count(std::chrono::hours(24));
double security_score = sec_monitor->calculate_security_score();
```

### Encryption Manager

Field-level and column-level data encryption:

```cpp
auto enc_mgr = context->get_encryption_manager();

// Master key management
enc_mgr->set_master_encryption_key("master-encryption-key-256bit");

// Column-level encryption configuration
enc_mgr->configure_encrypted_column("users", "ssn", security::encryption_type::aes256);
enc_mgr->configure_encrypted_column("users", "credit_card", security::encryption_type::aes256);

// Field data encryption/decryption
auto encrypted_ssn = enc_mgr->encrypt_field_data("123-45-6789", "ssn");
auto decrypted_ssn = enc_mgr->decrypt_field_data(encrypted_ssn, "ssn");

// Key rotation
enc_mgr->rotate_field_key("ssn");

// Check encryption status
bool is_encrypted = enc_mgr->is_column_encrypted("users", "ssn");
```

For production deployment details, see [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md).

---

## Performance Monitoring

**Status**: ✅ Full Support with monitoring_system integration
**Implementation**: `monitoring/`, see [BENCHMARKS.md](BENCHMARKS.md) for metrics

---

## Asynchronous Operations

**Status**: ✅ Full Support (C++20 coroutines optional, C++17 std::future fallback)
**Implementation**: `async/async_operations.h`

### C++20 Coroutines

```cpp
#include <database/async/async_operations.h>

// Async query with coroutines
database_awaitable<database_result> fetch_users_async() {
    auto db = co_await async_db_connect("host=localhost dbname=mydb");

    auto result = co_await db.execute_query_async(
        "SELECT * FROM users WHERE is_active = true"
    );

    co_return result;
}

// Use in async context
auto users = co_await fetch_users_async();
```

### C++20 Concepts Integration

The async operations now leverage C++20 concepts for compile-time type validation:

**Header**: `#include <database/core/concepts.h>`

**Available Concepts**:

| Concept | Description | Use Case |
|---------|-------------|----------|
| `SubmittableTask<F, Args...>` | Task callable for async executor | `async_executor.submit()` |
| `VoidCallable<F, Args...>` | Callback returning void | Completion handlers |
| `ErrorHandler<F>` | Exception handler callable | `on_error()` callbacks |
| `QueryCallback<F, ResultType>` | Query result handler | `on_query_complete()` |
| `StreamEventHandler<F, EventType>` | Stream event processor | Real-time data handlers |
| `StreamEventFilter<F, EventType>` | Event filtering predicate | Event filtering |
| `TransactionAction<F>` | Saga forward action | Distributed transactions |
| `CompensationAction<F>` | Saga rollback action | Compensation logic |

**Type-Safe Async Task Submission**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept-constrained task submission
template<SubmittableTask<database_result> F>
auto submit_query_task(async_executor& executor, F&& func) {
    return executor.submit(std::forward<F>(func));
}

// Usage - compiler validates callable signature at compile time
auto future = submit_query_task(executor, [&db]() {
    return db.select_query("SELECT * FROM users");
});
```

**Type-Safe Error Handling**:

```cpp
#include <database/core/concepts.h>
using namespace database::concepts;

// Concept-constrained error handler registration
template<ErrorHandler F>
void set_error_handler(F&& handler) {
    error_handler_ = std::forward<F>(handler);
}

// Usage - compiler validates exception handler signature
set_error_handler([](const std::exception& e) {
    log_error("Database error: " + std::string(e.what()));
});
```

**Saga Pattern with Concepts**:

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
saga_builder.add_step(
    []() { /* Create order */ },
    []() { /* Cancel order */ }
);
```

**Benefits**:
- **Clearer error messages**: Template errors shown as concept violations
- **Self-documenting code**: Type requirements expressed explicitly
- **Better IDE support**: Improved auto-completion and type hints
- **Backward compatible**: Legacy `std::function` overloads maintained

### C++17 Future-Based Async

```cpp
// Future-based async operations (C++17 fallback)
auto future_result = async_db.execute_async([&db]() {
    return db.select_query("SELECT * FROM large_table");
});

// Do other work...

// Get result when ready
auto result = future_result.get();
```

---

## Proxy Mode

**Status**: ✅ Full Support (Phase 4.1)
**Implementation**: `proxy/proxy_config.h`, `proxy/proxy_connector.h`

Proxy mode allows database_system clients to connect through a database_server middleware instead of directly to the database. This enables centralized connection management, security enforcement, and load balancing.

### Configuration

```cpp
#include <database/proxy/proxy_config.h>

database::proxy::proxy_connection_config config;
config.server_host = "db-gateway.internal";
config.server_port = 9432;
config.auth_token = "client-token-xyz";
config.connection_timeout = std::chrono::milliseconds{5000};
config.query_timeout = std::chrono::milliseconds{30000};
config.retry_count = 3;
config.retry_delay = std::chrono::milliseconds{1000};
config.use_tls = true;
config.ca_cert_path = "/etc/ssl/certs/ca.pem";

// Optional: mutual TLS (mTLS)
config.client_cert_path = "/etc/ssl/client.crt";
config.client_key_path = "/etc/ssl/client.key";

// Validate configuration
if (config.is_valid()) {
    // Use proxy connection
}
```

### Connection Modes

The system supports two connection modes:
- **Direct mode** (default): Direct connection to the database server
- **Proxy mode**: Connection through database_server middleware

```cpp
// Set connection mode to proxy
manager->set_connection_mode(connection_mode::proxy);
manager->configure_proxy(config);
```

---

## Unified Database System

**Status**: ✅ Full Support (Phase 6)
**Implementation**: `integrated/unified_database_system.h`, `integrated/core/database_coordinator.h`

The unified database system provides a zero-configuration entry point that integrates all adapters (logger, monitoring, thread) behind the scenes.

### Zero-Configuration Usage

```cpp
#include <database/integrated/unified_database_system.h>

using namespace database::integrated;

// Simplest usage - smart defaults
unified_database_system db;
auto result = db.connect("postgresql://localhost/mydb");
if (result.is_ok()) {
    auto rows = db.execute("SELECT * FROM users WHERE id = $1", 42);
}
```

### Builder Pattern Configuration

```cpp
auto db = unified_database_system::builder()
    .set_backend(backend_type::postgresql)
    .set_connection_string("host=localhost dbname=mydb")
    .set_pool_size(10, 50)
    .enable_logging(db_log_level::debug, "./logs")
    .enable_monitoring(true)
    .enable_async(4)  // 4 worker threads
    .build();

// Async query execution
auto future = db->execute_async("SELECT * FROM large_table");
// Do other work...
auto result = future.get();

// Transaction management
auto tx = db->begin_transaction();
tx->execute("INSERT INTO users (name) VALUES ($1)", "Alice");
tx->execute("UPDATE accounts SET balance = balance - 100");
tx->commit();
```

### Integrated Adapters

The coordinator integrates the following adapter backends:
- **Logger adapter**: Structured logging with configurable backends (system logger, null logger, fallback)
- **Monitoring adapter**: Performance metrics collection with optional monitoring_system integration
- **Thread adapter**: Async operation support with optional thread_system integration

---

## common_system Integration

**Status**: ✅ Full Support
**Implementation**: `include/kcenon/database/adapters/common_system_database_adapter.h`, `include/kcenon/database/di/service_registration.h`

When built with common_system (via `KCENON_HAS_COMMON_SYSTEM` feature flag), database_system provides adapter and DI integration.

### IDatabase Adapter

Bridges common_system's `IDatabase` interface with database_system's `database_manager`:

```cpp
#include <kcenon/database/adapters/common_system_database_adapter.h>

using namespace kcenon::database::adapters;

// Create adapter with specific database type
auto adapter = std::make_shared<common_system_database_adapter>(
    ::database::database_types::postgresql);

// Use through common_system IDatabase interface
auto connect_result = adapter->connect("host=localhost dbname=mydb");
auto query_result = adapter->execute_query("SELECT * FROM users");
auto cmd_result = adapter->execute_command("INSERT INTO logs VALUES (...)");

// Transaction support
adapter->begin_transaction();
adapter->execute_command("UPDATE accounts SET balance = balance - 100");
adapter->commit();

// Access underlying database_manager for advanced features
auto manager = adapter->get_manager();
```

### Service Container Registration

Register database services with common_system's dependency injection container:

```cpp
#include <kcenon/database/di/service_registration.h>

using namespace kcenon::database::di;

auto& container = common::di::service_container::global();

// Register with default configuration (PostgreSQL, singleton)
auto result = register_database_services(container);

// Or with custom configuration
database_registration_config config;
config.db_type = ::database::database_types::sqlite;
config.connection_string = "database.db";
config.connect_on_register = true;
config.lifetime = common::di::service_lifetime::singleton;
auto result = register_database_services(container, config);

// Resolve database anywhere in the application
auto db = container.resolve<common::interfaces::IDatabase>().value();
db->connect("host=localhost dbname=mydb");
auto query_result = db->execute_query("SELECT * FROM users");
```

### Feature Flags

Build configuration is controlled via unified feature flags:

```cpp
#include <kcenon/database/config/feature_flags.h>

#if KCENON_HAS_COMMON_SYSTEM
    // Use common_system Result<T>, IDatabase, DI container
#else
    // Use local fallbacks
#endif
```

---

## C++20 Modules

**Status**: ✅ Full Support
**Implementation**: `src/modules/database.cppm`

The database_system can be consumed as a C++20 module for faster compilation and better encapsulation.

### Module Structure

| Module | Partition | Contents |
|--------|-----------|----------|
| `kcenon.database` | (primary) | Aggregates all partitions |
| `kcenon.database:core` | Core | Types, context, manager, backend registry, proxy config |
| `kcenon.database:query` | Query | Query builder, conditions, dialects (SQL, MongoDB, Redis) |
| `kcenon.database:backends` | Backends | PostgreSQL, SQLite, MongoDB, Redis backends |

### Usage

```cpp
import kcenon.database;

using namespace database;

// Create database context and manager
auto context = std::make_shared<database_context>();
auto manager = std::make_shared<database_manager>(context);

// Configure and connect
manager->set_mode(database_types::postgres);
auto result = manager->connect_result("host=localhost dbname=test");
if (result.is_ok()) {
    auto query_result = manager->select_query_result("SELECT * FROM users");
}

// Use query builder
auto builder = manager->create_query_builder();
auto query = builder
    .select({"id", "name"})
    .from("users")
    .where("active", "=", true)
    .limit(10)
    .build();
```

### Dependencies

```
kcenon.database
  ├── kcenon.common (Tier 0) - Result<T>, error handling
  ├── kcenon.thread (Tier 1) - Thread pool for async operations (optional)
  └── kcenon.container (Tier 1) - Serialization (optional)
```

---

## Technology Stack

### Dependencies

**Required**:
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+

**Optional** (for specific backends):
- libpqxx (PostgreSQL)
- sqlite3 (SQLite)
- mongo-cxx-driver (MongoDB)
- hiredis (Redis)

**Optional** (for enhanced features):
- thread_system (connection pooling v3, async operations)
- monitoring_system (performance metrics, Prometheus export)
- logger_system (structured logging)

### CMake Integration

See [Project Structure](PROJECT_STRUCTURE.md) for build configuration.

---

**For comprehensive performance benchmarks**, see [BENCHMARKS.md](BENCHMARKS.md)
**For production quality details**, see [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md)
**For project organization**, see [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md)

---

**Last Updated**: 2026-02-08
**Maintained by**: kcenon@naver.com
