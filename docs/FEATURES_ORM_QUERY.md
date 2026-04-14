---
doc_id: "DBS-FEAT-002-ORM-QUERY"
doc_title: "Database System Features - ORM and Query Builders"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "database_system"
category: "FEAT"
---

# Database System Features - ORM and Query Builders

> **SSOT**: This document is a focused sub-document of **Database System Features**, covering the ORM framework and query builders.

**Last Updated**: 2026-02-08
**Version**: 0.4.0.0

This document provides comprehensive details on the ORM framework (entity definition, operations, relationships, schema management, advanced features) and the query builder APIs (immutable, SQL, NoSQL).

---

## Table of Contents

- [ORM Framework](#orm-framework)
  - [Entity Definition](#entity-definition)
  - [Entity Operations](#entity-operations)
  - [Relationships](#relationships)
  - [Schema Management](#schema-management)
  - [Advanced ORM Features](#advanced-orm-features)
- [Query Builders](#query-builders)
  - [Immutable Query Builder](#immutable-query-builder)
  - [SQL Query Builder](#sql-query-builder)
  - [NoSQL Query Builder](#nosql-query-builder)

---

## ORM Framework

**Status**: Full Support (C++20 concepts-based)
**Implementation**: `orm/entity.h`, `orm/entity_manager.h`, `orm/schema_manager.h`

### Entity Definition

Define database entities using C++20 concepts-based macros:

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

## Query Builders

**Status**: Full Support
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

## Related Documents

- [FEATURES.md](FEATURES.md) - Features index
- [FEATURES_BACKENDS.md](FEATURES_BACKENDS.md) - PostgreSQL, SQLite, MongoDB, Redis backend docs
- [FEATURES_POOLING_SECURITY.md](FEATURES_POOLING_SECURITY.md) - Connection pooling, security, and monitoring
- [ORM_GUIDE.md](ORM_GUIDE.md) - In-depth ORM guide

---

**Last Updated**: 2026-02-08
