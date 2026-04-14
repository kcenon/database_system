---
doc_id: "DBS-FEAT-002-BACKENDS"
doc_title: "Database System Features - Backends"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "FEAT"
---

# Database System Features - Backends

> **SSOT**: This document is a focused sub-document of **Database System Features**, covering backend implementations.

> **See also**: [ADAPTER_PATTERNS.md](ADAPTER_PATTERNS.md) — the adapter pattern used to keep backend integration decoupled from optional dependencies.

**Last Updated**: 2026-04-15
**Version**: 0.4.1.0

This document provides comprehensive details on all supported database backends: PostgreSQL, SQLite, MongoDB, and Redis.

---

## Table of Contents

- [Multi-Backend Support](#multi-backend-support)
  - [PostgreSQL Backend](#postgresql-backend)
  - [SQLite Backend](#sqlite-backend)
  - [MongoDB Backend](#mongodb-backend)
  - [Redis Backend](#redis-backend)

---

## Multi-Backend Support

The database_system provides unified access to multiple database backends with consistent API.

### PostgreSQL Backend

**Status**: Full Support
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

**Status**: Full Support
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

**Status**: Experimental (disabled by default)
**Implementation**: `mongodb/mongodb_manager.h/cpp`

> **Experimental**: MongoDB support is functional but experimental. Enable with `USE_MONGODB=ON` CMake option.

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

**Status**: Experimental (disabled by default)
**Implementation**: `redis/redis_manager.h/cpp`

> **Experimental**: Redis support is functional but experimental. Enable with `USE_REDIS=ON` CMake option.

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

## Related Documents

- [FEATURES.md](FEATURES.md) - Features index
- [FEATURES_ORM_QUERY.md](FEATURES_ORM_QUERY.md) - ORM framework and query builders
- [FEATURES_POOLING_SECURITY.md](FEATURES_POOLING_SECURITY.md) - Connection pooling, security, and monitoring
- [BENCHMARKS.md](BENCHMARKS.md) - Performance benchmarks
- [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md) - Production quality details

---

**Last Updated**: 2026-02-08
