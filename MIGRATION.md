# Database System Migration Guide

> **Version:** 1.0.0
> **Last Updated:** 2025-10-22
> **Language:** English

## Table of Contents

- [Overview](#overview)
- [Migration from Raw SQL to ORM](#migration-from-raw-sql-to-orm)
- [Migration Between Database Backends](#migration-between-database-backends)
- [Schema Migration Management](#schema-migration-management)
- [Common Migration Issues](#common-migration-issues)
- [Best Practices](#best-practices)

---

## Overview

This guide provides comprehensive information for migrating database operations in the Database System. It covers migrating from raw SQL to the ORM framework, switching between database backends, managing schema changes, and resolving common migration issues.

### Migration Scenarios

1. **Raw SQL to ORM**: Modernize legacy code with type-safe entity mapping
2. **Backend Migration**: Switch from one database type to another (e.g., MySQL to PostgreSQL)
3. **Schema Evolution**: Manage database schema changes over time
4. **Performance Optimization**: Migrate from single connections to connection pooling
5. **Security Enhancement**: Add authentication, encryption, and audit logging

---

## Migration from Raw SQL to ORM

### Phase 1: Analyze Existing Code

Identify patterns in your raw SQL code:

```cpp
// Legacy raw SQL approach
database_manager& db = database_manager::handle();
db.set_mode(database_types::postgres);
db.connect("host=localhost dbname=mydb user=myuser");

// Raw SQL queries
auto result = db.select_query(
    "SELECT id, username, email, created_at FROM users WHERE age > 18"
);

for (const auto& row : result) {
    int id = std::get<int64_t>(row.at("id"));
    std::string username = std::get<std::string>(row.at("username"));
    std::string email = std::get<std::string>(row.at("email"));
    // Manual field extraction...
}
```

### Phase 2: Define Entity Classes

Create entity classes using the ORM framework:

```cpp
#include <database/orm/entity.h>

namespace database::orm {

// Define User entity
class User : public entity_base {
public:
    using primary_key_type = int64_t;

    int64_t id;
    std::string username;
    std::string email;
    int age;
    std::chrono::system_clock::time_point created_at;

    // Table name
    static std::string table_name() { return "users"; }

    // Metadata generation
    static const entity_metadata& get_metadata() {
        static entity_metadata metadata("users");
        static bool initialized = false;

        if (!initialized) {
            metadata.add_field(field_metadata(
                "id", "int64_t",
                field_constraint::primary_key | field_constraint::auto_increment
            ));
            metadata.add_field(field_metadata(
                "username", "string",
                field_constraint::not_null | field_constraint::index,
                "idx_username"
            ));
            metadata.add_field(field_metadata(
                "email", "string",
                field_constraint::unique | field_constraint::not_null
            ));
            metadata.add_field(field_metadata(
                "age", "int32_t",
                field_constraint::not_null
            ));
            metadata.add_field(field_metadata(
                "created_at", "timestamp",
                field_constraint::default_now
            ));
            initialized = true;
        }

        return metadata;
    }
};

}
```

### Phase 3: Migrate Queries to ORM

Replace raw SQL with type-safe ORM queries:

```cpp
#include <database/orm/entity.h>

// Modern ORM approach
auto users = database::orm::query_builder<User>(db)
    .where("age", ">", database_value{int64_t(18)})
    .order_by("created_at", sort_order::desc)
    .execute();

if (users) {
    for (const auto& row : *users) {
        // Type-safe field access
        User user;
        user.id = std::get<int64_t>(row.at("id"));
        user.username = std::get<std::string>(row.at("username"));
        user.email = std::get<std::string>(row.at("email"));
        user.age = std::get<int64_t>(row.at("age"));
        // ...
    }
}
```

### Phase 4: Schema Generation

Generate database schema from entity definitions:

```cpp
// Automatic schema creation
const auto& metadata = User::get_metadata();

// Generate CREATE TABLE statement
std::string create_table_sql = metadata.create_table_sql();
db.execute_query(create_table_sql);

// Generate index creation statements
for (const auto& index_sql : metadata.create_index_sql()) {
    db.execute_query(index_sql);
}
```

### Migration Checklist

- [ ] Identify all raw SQL queries in codebase
- [ ] Create entity classes for each table
- [ ] Define field metadata with constraints
- [ ] Replace SELECT queries with ORM queries
- [ ] Replace INSERT queries with ORM insert operations
- [ ] Replace UPDATE queries with ORM update operations
- [ ] Replace DELETE queries with ORM delete operations
- [ ] Add indexes and foreign keys via metadata
- [ ] Test all migrated operations
- [ ] Remove deprecated raw SQL code

---

## Migration Between Database Backends

### PostgreSQL to MySQL Migration

#### 1. Data Type Mapping

| PostgreSQL | MySQL | Notes |
|------------|-------|-------|
| SERIAL | INT AUTO_INCREMENT | Auto-incrementing integer |
| BIGSERIAL | BIGINT AUTO_INCREMENT | Auto-incrementing big integer |
| TEXT | TEXT or VARCHAR(max) | Text fields |
| TIMESTAMP | DATETIME | Date/time storage |
| BOOLEAN | TINYINT(1) | Boolean values |
| JSONB | JSON | JSON data (no binary storage in MySQL) |
| ARRAY | JSON | Arrays stored as JSON |

#### 2. SQL Syntax Differences

**PostgreSQL**:
```sql
-- Parameterized queries
SELECT * FROM users WHERE id = $1;

-- RETURNING clause
INSERT INTO users (username) VALUES ('john') RETURNING id;

-- LIMIT/OFFSET
SELECT * FROM users LIMIT 10 OFFSET 20;
```

**MySQL**:
```sql
-- Parameterized queries
SELECT * FROM users WHERE id = ?;

-- LAST_INSERT_ID()
INSERT INTO users (username) VALUES ('john');
SELECT LAST_INSERT_ID();

-- LIMIT/OFFSET
SELECT * FROM users LIMIT 20, 10;
```

#### 3. Code Migration

```cpp
// Before: PostgreSQL-specific
db.set_mode(database_types::postgres);
db.connect("host=localhost port=5432 dbname=mydb user=myuser");

auto result = db.select_query(
    "SELECT * FROM users WHERE id = $1 RETURNING *",
    {database_value{int64_t(42)}}
);

// After: MySQL-compatible
db.set_mode(database_types::mysql);
db.connect("host=localhost;database=mydb;user=myuser;password=mypass");

auto result = db.select_query(
    "SELECT * FROM users WHERE id = ?",
    {database_value{int64_t(42)}}
);
```

### MySQL to PostgreSQL Migration

#### 1. Leverage Advanced Features

```cpp
// MySQL: Limited JSON support
db.execute_query(
    "INSERT INTO products (data) VALUES ('{\"name\":\"Widget\",\"price\":19.99}')"
);

// PostgreSQL: JSONB with indexing
db.execute_query(
    "CREATE INDEX idx_product_name ON products USING GIN ((data->'name'))"
);
db.execute_query(
    "INSERT INTO products (data) VALUES ('{\"name\":\"Widget\",\"price\":19.99}'::jsonb)"
);
```

#### 2. Array Support

```cpp
// PostgreSQL: Native array support
db.execute_query(
    "CREATE TABLE tags (id SERIAL, values TEXT[])"
);
db.execute_query(
    "INSERT INTO tags (values) VALUES (ARRAY['tag1', 'tag2', 'tag3'])"
);

// MySQL: Use JSON instead
db.execute_query(
    "CREATE TABLE tags (id INT AUTO_INCREMENT PRIMARY KEY, values JSON)"
);
db.execute_query(
    "INSERT INTO tags (values) VALUES ('[\"tag1\", \"tag2\", \"tag3\"]')"
);
```

### SQLite to PostgreSQL/MySQL Migration

#### 1. Transaction Support

```cpp
// SQLite: Limited concurrency
db.set_mode(database_types::sqlite);
db.begin_transaction();
// Only one writer at a time

// PostgreSQL/MySQL: Full MVCC concurrency
db.set_mode(database_types::postgres);
db.begin_transaction();
// Multiple concurrent writers supported
```

#### 2. Data Type Flexibility

```cpp
// SQLite: Dynamic typing
db.execute_query("CREATE TABLE flexible (data)");

// PostgreSQL/MySQL: Strict typing
db.execute_query("CREATE TABLE strict (data VARCHAR(255))");
```

### SQL to NoSQL Migration

#### MongoDB Migration

```cpp
// Before: SQL relational approach
db.set_mode(database_types::postgres);
db.execute_query(
    "SELECT u.id, u.name, o.order_id, o.total "
    "FROM users u JOIN orders o ON u.id = o.user_id"
);

// After: MongoDB document approach
db.set_mode(database_types::mongodb);
db.execute_query(
    R"({
        "aggregate": "users",
        "pipeline": [
            {
                "$lookup": {
                    "from": "orders",
                    "localField": "_id",
                    "foreignField": "user_id",
                    "as": "orders"
                }
            }
        ]
    })"
);
```

#### Redis Migration (Caching Layer)

```cpp
// Add Redis as a cache layer
connection_pool_config redis_config;
redis_config.connection_string = "redis://localhost:6379/0";
db.create_connection_pool(database_types::redis, redis_config);

// Write-through cache pattern
void save_user(const User& user) {
    // Save to primary database (PostgreSQL)
    db.set_mode(database_types::postgres);
    db.execute_query("INSERT INTO users ...");

    // Cache in Redis
    db.set_mode(database_types::redis);
    db.execute_query(
        "HSET user:" + std::to_string(user.id) +
        " name " + user.name +
        " email " + user.email
    );
}

// Read-through cache pattern
std::optional<User> get_user(int64_t user_id) {
    // Try Redis cache first
    db.set_mode(database_types::redis);
    auto cached = db.select_query("HGETALL user:" + std::to_string(user_id));
    if (!cached.empty()) {
        return parse_user(cached);
    }

    // Fallback to PostgreSQL
    db.set_mode(database_types::postgres);
    auto result = db.select_query("SELECT * FROM users WHERE id = " + std::to_string(user_id));
    return parse_user(result);
}
```

---

## Schema Migration Management

### Version-Controlled Migrations

#### 1. Migration File Structure

```
migrations/
├── 001_create_users_table.sql
├── 002_add_email_index.sql
├── 003_create_orders_table.sql
├── 004_add_foreign_keys.sql
└── 005_add_audit_columns.sql
```

#### 2. Migration Tracking Table

```sql
CREATE TABLE schema_migrations (
    version INTEGER PRIMARY KEY,
    description VARCHAR(255) NOT NULL,
    applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    checksum VARCHAR(64)
);
```

#### 3. Migration Runner

```cpp
class migration_manager {
public:
    migration_manager(database_manager& db) : db_(db) {}

    void run_migrations(const std::string& migrations_dir) {
        auto applied = get_applied_migrations();
        auto pending = get_pending_migrations(migrations_dir, applied);

        for (const auto& migration : pending) {
            std::cout << "Applying migration: " << migration.version
                      << " - " << migration.description << std::endl;

            db_.begin_transaction();
            try {
                execute_migration_file(migration.file_path);
                record_migration(migration);
                db_.commit_transaction();
                std::cout << "Migration applied successfully" << std::endl;
            } catch (const std::exception& e) {
                db_.rollback_transaction();
                std::cerr << "Migration failed: " << e.what() << std::endl;
                throw;
            }
        }
    }

private:
    database_manager& db_;

    std::vector<int> get_applied_migrations() {
        auto result = db_.select_query(
            "SELECT version FROM schema_migrations ORDER BY version"
        );

        std::vector<int> versions;
        for (const auto& row : result) {
            versions.push_back(std::get<int64_t>(row.at("version")));
        }
        return versions;
    }

    void execute_migration_file(const std::string& file_path) {
        std::ifstream file(file_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        db_.execute_query(buffer.str());
    }

    void record_migration(const migration_info& migration) {
        db_.execute_query(
            "INSERT INTO schema_migrations (version, description, checksum) "
            "VALUES (" + std::to_string(migration.version) + ", "
            "'" + migration.description + "', "
            "'" + migration.checksum + "')"
        );
    }
};
```

### ORM-Based Schema Migrations

```cpp
// Automatic schema synchronization
class schema_synchronizer {
public:
    void sync_schema(database_base* db, const entity_metadata& metadata) {
        if (!table_exists(db, metadata.table_name())) {
            create_table(db, metadata);
        } else {
            update_table(db, metadata);
        }

        sync_indexes(db, metadata);
        sync_foreign_keys(db, metadata);
    }

private:
    bool table_exists(database_base* db, const std::string& table) {
        auto result = db->select_query(
            "SELECT COUNT(*) FROM information_schema.tables "
            "WHERE table_name = '" + table + "'"
        );
        return std::get<int64_t>(result[0].at("count")) > 0;
    }

    void create_table(database_base* db, const entity_metadata& metadata) {
        db->execute_query(metadata.create_table_sql());
    }

    void update_table(database_base* db, const entity_metadata& metadata) {
        // Compare existing schema with metadata
        auto existing_columns = get_table_columns(db, metadata.table_name());
        auto new_columns = metadata.fields();

        // Add missing columns
        for (const auto& field : new_columns) {
            if (existing_columns.find(field.name()) == existing_columns.end()) {
                add_column(db, metadata.table_name(), field);
            }
        }
    }
};
```

### Safe Schema Changes

#### Adding Columns

```cpp
// Safe: Add nullable column
db.execute_query(
    "ALTER TABLE users ADD COLUMN phone_number VARCHAR(20)"
);

// Then make it NOT NULL in a separate migration
db.execute_query(
    "UPDATE users SET phone_number = '' WHERE phone_number IS NULL"
);
db.execute_query(
    "ALTER TABLE users ALTER COLUMN phone_number SET NOT NULL"
);
```

#### Renaming Columns

```cpp
// PostgreSQL
db.execute_query(
    "ALTER TABLE users RENAME COLUMN old_name TO new_name"
);

// MySQL
db.execute_query(
    "ALTER TABLE users CHANGE old_name new_name VARCHAR(255)"
);

// SQLite (requires table rebuild)
db.execute_query("ALTER TABLE users RENAME TO users_old");
db.execute_query("CREATE TABLE users (...)");
db.execute_query("INSERT INTO users SELECT ... FROM users_old");
db.execute_query("DROP TABLE users_old");
```

#### Changing Data Types

```cpp
// Safe approach: Create new column, migrate data, drop old
db.execute_query("ALTER TABLE users ADD COLUMN id_new BIGINT");
db.execute_query("UPDATE users SET id_new = id::BIGINT");
db.execute_query("ALTER TABLE users DROP COLUMN id");
db.execute_query("ALTER TABLE users RENAME COLUMN id_new TO id");
```

---

## Common Migration Issues

### Issue 1: Character Encoding

**Problem**: Data corruption when migrating between databases with different encodings.

**Solution**:
```cpp
// PostgreSQL: Set UTF-8 encoding
db.connect("host=localhost dbname=mydb client_encoding=UTF8");

// MySQL: Set UTF-8 encoding
db.connect("host=localhost;database=mydb;charset=utf8mb4");

// Verify encoding
auto result = db.select_query("SHOW VARIABLES LIKE 'character_set%'");
```

### Issue 2: Primary Key Conflicts

**Problem**: Auto-increment sequences out of sync after data import.

**Solution**:
```cpp
// PostgreSQL: Reset sequence
db.execute_query(
    "SELECT setval('users_id_seq', (SELECT MAX(id) FROM users))"
);

// MySQL: Reset auto_increment
db.execute_query(
    "ALTER TABLE users AUTO_INCREMENT = 1"
);
```

### Issue 3: Foreign Key Constraint Violations

**Problem**: Cannot add foreign keys due to existing orphaned records.

**Solution**:
```cpp
// Find orphaned records
auto orphans = db.select_query(
    "SELECT o.* FROM orders o "
    "LEFT JOIN users u ON o.user_id = u.id "
    "WHERE u.id IS NULL"
);

// Clean up orphaned records
db.execute_query("DELETE FROM orders WHERE user_id NOT IN (SELECT id FROM users)");

// Now add foreign key
db.execute_query(
    "ALTER TABLE orders ADD CONSTRAINT fk_orders_users "
    "FOREIGN KEY (user_id) REFERENCES users(id)"
);
```

### Issue 4: Transaction Deadlocks

**Problem**: Deadlocks during large data migrations.

**Solution**:
```cpp
// Process in smaller batches
const size_t batch_size = 1000;
size_t offset = 0;

while (true) {
    db.begin_transaction();

    auto batch = db.select_query(
        "SELECT * FROM old_table LIMIT " + std::to_string(batch_size) +
        " OFFSET " + std::to_string(offset)
    );

    if (batch.empty()) break;

    for (const auto& row : batch) {
        db.execute_query("INSERT INTO new_table ...");
    }

    db.commit_transaction();
    offset += batch_size;

    // Brief pause to reduce lock contention
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

### Issue 5: Large Data Migration Timeouts

**Problem**: Migration fails due to connection timeouts with large datasets.

**Solution**:
```cpp
// Increase connection timeout
connection_pool_config config;
config.connection_string = "host=localhost dbname=mydb connect_timeout=300";
config.acquire_timeout = std::chrono::seconds(300);

db.create_connection_pool(database_types::postgres, config);

// Use async operations for very large migrations
auto future = async_db.execute_async("INSERT INTO new_table SELECT * FROM old_table");
while (future.wait_for(std::chrono::seconds(1)) != std::future_status::ready) {
    std::cout << "Migration in progress..." << std::endl;
}
```

### Issue 6: Index Rebuild Performance

**Problem**: Creating indexes on large tables takes too long.

**Solution**:
```cpp
// PostgreSQL: Create index concurrently
db.execute_query(
    "CREATE INDEX CONCURRENTLY idx_users_email ON users(email)"
);

// Or create index after data load
db.execute_query("DROP INDEX IF EXISTS idx_users_email");
// Bulk insert data...
db.execute_query("CREATE INDEX idx_users_email ON users(email)");
```

---

## Best Practices

### 1. Always Use Transactions

```cpp
// Wrap migrations in transactions
db.begin_transaction();
try {
    // Execute migration steps
    db.execute_query("ALTER TABLE ...");
    db.execute_query("UPDATE ...");
    db.commit_transaction();
} catch (const std::exception& e) {
    db.rollback_transaction();
    std::cerr << "Migration failed: " << e.what() << std::endl;
    throw;
}
```

### 2. Backup Before Migration

```bash
# PostgreSQL
pg_dump -h localhost -U myuser mydb > backup_before_migration.sql

# MySQL
mysqldump -h localhost -u myuser -p mydb > backup_before_migration.sql

# SQLite
sqlite3 mydb.db ".backup backup_before_migration.db"
```

### 3. Test Migrations

```cpp
// Test on a copy of production data
class migration_tester {
public:
    void test_migration(const std::string& migration_file) {
        // Create test database
        create_test_database();

        // Copy sample data
        copy_production_sample();

        // Run migration
        migration_manager migrator(test_db);
        migrator.run_migration(migration_file);

        // Verify data integrity
        verify_data_integrity();

        // Clean up
        drop_test_database();
    }
};
```

### 4. Document Schema Changes

```sql
-- migrations/005_add_audit_columns.sql
-- Description: Add audit columns for tracking record changes
-- Author: kcenon@naver.com
-- Date: 2025-10-22
-- Dependencies: 004_add_foreign_keys.sql

ALTER TABLE users ADD COLUMN created_by VARCHAR(50);
ALTER TABLE users ADD COLUMN created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;
ALTER TABLE users ADD COLUMN updated_by VARCHAR(50);
ALTER TABLE users ADD COLUMN updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;

-- Create trigger for automatic update timestamp
CREATE TRIGGER update_users_timestamp
BEFORE UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();
```

### 5. Monitor Migration Progress

```cpp
class migration_monitor {
public:
    void monitor_large_migration() {
        std::thread monitor_thread([this]() {
            while (!migration_complete_) {
                auto stats = get_migration_stats();
                std::cout << "Rows migrated: " << stats.rows_migrated
                          << " / " << stats.total_rows
                          << " (" << (stats.rows_migrated * 100.0 / stats.total_rows)
                          << "%)" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });

        // Run migration
        run_migration();
        migration_complete_ = true;
        monitor_thread.join();
    }
};
```

### 6. Rollback Plan

```cpp
// Always have a rollback migration
// migrations/005_add_audit_columns.sql (up migration)
// migrations/005_add_audit_columns_rollback.sql (down migration)

class migration_manager {
public:
    void rollback_migration(int version) {
        auto rollback_file = find_rollback_migration(version);
        if (!rollback_file) {
            throw std::runtime_error("No rollback available for version " +
                                     std::to_string(version));
        }

        db_.begin_transaction();
        try {
            execute_migration_file(*rollback_file);
            remove_migration_record(version);
            db_.commit_transaction();
        } catch (const std::exception& e) {
            db_.rollback_transaction();
            throw;
        }
    }
};
```

---

## Conclusion

Database migrations are critical operations that require careful planning, testing, and execution. This guide provides a comprehensive foundation for successful migrations in the Database System.

### Migration Checklist

- [ ] Identify migration type (raw SQL to ORM, backend switch, schema change)
- [ ] Create comprehensive backup
- [ ] Test migration on staging environment
- [ ] Document all schema changes
- [ ] Prepare rollback plan
- [ ] Monitor migration progress
- [ ] Verify data integrity after migration
- [ ] Update application code to match new schema
- [ ] Update documentation

### Additional Resources

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture overview
- [STRUCTURE.md](STRUCTURE.md) - Project directory structure
- [docs/API_REFERENCE.md](docs/API_REFERENCE.md) - Complete API documentation
- [samples/](samples/) - Example migrations and usage patterns

---

**Last Updated**: 2025-10-22
**Version**: 1.0.0
**Maintainer**: kcenon@naver.com
