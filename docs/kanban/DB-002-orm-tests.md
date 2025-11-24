# DB-002: ORM Advanced Feature Tests

**Category**: TEST
**Priority**: HIGH
**Status**: DONE
**Est. Duration**: 5-7 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- `query_builder` class is implemented but lacks tests for advanced features
- Complex query builder features like JOIN, GROUP BY, subqueries are not verified
- Tests for MongoDB/Redis query builders are missing
- Query compatibility across multiple database types is not verified

### Target State
- Complete API tests for `sql_query_builder`, `mongodb_query_builder`, `redis_query_builder`
- Tests for complex query scenarios (JOIN, aggregation, subqueries)
- Cross-database query builder compatibility verification

### Scope
**Target Files**:
- `database/query_builder.h`
- `database/query_builder.cpp`

**Test Files to Add/Modify**:
- `tests/sql_query_builder_test.cpp`
- `tests/mongodb_query_builder_test.cpp`
- `tests/redis_query_builder_test.cpp`
- `tests/universal_query_builder_test.cpp`

---

## 2. How to Change

### 2.1 SQL Query Builder Advanced Tests

```cpp
// tests/sql_query_builder_test.cpp
#include <gtest/gtest.h>
#include "database/query_builder.h"

class SQLQueryBuilderTest : public ::testing::Test {
protected:
    database::sql_query_builder builder_;
};

// JOIN Tests
TEST_F(SQLQueryBuilderTest, InnerJoin) {
    auto query = builder_
        .select({"u.id", "u.name", "o.total"})
        .from("users u")
        .join("orders o", "u.id = o.user_id", database::join_type::inner)
        .build();

    EXPECT_EQ(query,
        "SELECT u.id, u.name, o.total FROM users u "
        "INNER JOIN orders o ON u.id = o.user_id");
}

TEST_F(SQLQueryBuilderTest, LeftJoinWithCondition) {
    auto query = builder_
        .select({"*"})
        .from("users")
        .left_join("profiles", "users.id = profiles.user_id")
        .where("users.active", "=", true)
        .build();

    // Verify LEFT JOIN syntax
}

TEST_F(SQLQueryBuilderTest, MultipleJoins) {
    // Test chaining multiple joins
}

// GROUP BY & HAVING Tests
TEST_F(SQLQueryBuilderTest, GroupByWithHaving) {
    auto query = builder_
        .select({"department", "COUNT(*) as count"})
        .from("employees")
        .group_by("department")
        .having("COUNT(*) > 5")
        .build();

    EXPECT_TRUE(query.find("GROUP BY") != std::string::npos);
    EXPECT_TRUE(query.find("HAVING") != std::string::npos);
}

// Complex WHERE Conditions
TEST_F(SQLQueryBuilderTest, NestedConditions) {
    database::query_condition cond1("age", ">", 18);
    database::query_condition cond2("status", "=", std::string("active"));
    auto combined = cond1 && cond2;

    auto query = builder_
        .select({"*"})
        .from("users")
        .where(combined)
        .build();

    // Verify AND condition
}

// ORDER BY & LIMIT
TEST_F(SQLQueryBuilderTest, OrderByMultipleColumns) {
    auto query = builder_
        .select({"*"})
        .from("products")
        .order_by("category", database::sort_order::asc)
        .order_by("price", database::sort_order::desc)
        .limit(10)
        .offset(20)
        .build();

    EXPECT_TRUE(query.find("ORDER BY") != std::string::npos);
    EXPECT_TRUE(query.find("LIMIT 10") != std::string::npos);
}

// Database-Specific Generation
TEST_F(SQLQueryBuilderTest, PostgreSQLSpecificSyntax) {
    builder_.select({"*"}).from("users").limit(10);
    auto query = builder_.build_for_database(database::database_types::PostgreSQL);
    // PostgreSQL uses standard LIMIT
}

TEST_F(SQLQueryBuilderTest, MySQLSpecificSyntax) {
    builder_.select({"*"}).from("users").limit(10);
    auto query = builder_.build_for_database(database::database_types::MySQL);
    // MySQL uses backticks for identifiers
}
```

### 2.2 MongoDB Query Builder Tests

```cpp
// tests/mongodb_query_builder_test.cpp
#include <gtest/gtest.h>
#include "database/query_builder.h"

class MongoDBQueryBuilderTest : public ::testing::Test {
protected:
    database::mongodb_query_builder builder_;
};

// Find Operations
TEST_F(MongoDBQueryBuilderTest, FindWithFilter) {
    auto query = builder_
        .collection("users")
        .find({{"status", std::string("active")}})
        .build();

    // Verify MongoDB find syntax
}

TEST_F(MongoDBQueryBuilderTest, FindWithProjection) {
    auto query = builder_
        .collection("users")
        .find({})
        .project({"name", "email"})
        .exclude({"_id"})
        .build();
}

// Aggregation Pipeline
TEST_F(MongoDBQueryBuilderTest, AggregationPipeline) {
    auto query = builder_
        .collection("orders")
        .match({{"status", std::string("completed")}})
        .group({{"_id", std::string("$category")}})
        .build();
}

// Insert/Update/Delete
TEST_F(MongoDBQueryBuilderTest, InsertOne) { /* ... */ }
TEST_F(MongoDBQueryBuilderTest, UpdateMany) { /* ... */ }
TEST_F(MongoDBQueryBuilderTest, DeleteWithFilter) { /* ... */ }
```

### 2.3 Redis Query Builder Tests

```cpp
// tests/redis_query_builder_test.cpp
#include <gtest/gtest.h>
#include "database/query_builder.h"

class RedisQueryBuilderTest : public ::testing::Test {
protected:
    database::redis_query_builder builder_;
};

// String Operations
TEST_F(RedisQueryBuilderTest, SetAndGet) {
    builder_.set("key", "value");
    EXPECT_EQ(builder_.build(), "SET key value");

    builder_.reset();
    builder_.get("key");
    EXPECT_EQ(builder_.build(), "GET key");
}

// Hash Operations
TEST_F(RedisQueryBuilderTest, HashOperations) {
    builder_.hset("user:1", "name", "John");
    EXPECT_EQ(builder_.build(), "HSET user:1 name John");
}

// List Operations
TEST_F(RedisQueryBuilderTest, ListOperations) {
    builder_.lpush("queue", "task1");
    EXPECT_EQ(builder_.build(), "LPUSH queue task1");
}

// Expiration
TEST_F(RedisQueryBuilderTest, ExpireCommand) {
    builder_.expire("session", 3600);
    EXPECT_EQ(builder_.build(), "EXPIRE session 3600");
}
```

### 2.4 Implementation Steps

1. **SQL Query Builder Tests** (Days 1-2)
   - JOIN tests (5 types)
   - GROUP BY/HAVING tests (4 tests)
   - Complex WHERE condition tests (6 tests)
   - INSERT/UPDATE/DELETE tests (9 tests)
   - Database-specific syntax generation tests (3 tests)

2. **MongoDB Query Builder Tests** (Days 3-4)
   - CRUD operation tests (8 tests)
   - Aggregation pipeline tests (5 tests)
   - JSON generation accuracy tests (4 tests)

3. **Redis Query Builder Tests** (Day 5)
   - String command tests (4 tests)
   - Hash command tests (4 tests)
   - List/Set command tests (8 tests)
   - TTL command tests (2 tests)

4. **Universal Query Builder Tests** (Days 6-7)
   - Automatic builder selection tests
   - Interface integration tests
   - Error case tests

---

## 3. How to Test

### 3.1 Unit Test Execution

```bash
# SQL Query Builder tests
ctest -R sql_query_builder_test -V

# MongoDB Query Builder tests
ctest -R mongodb_query_builder_test -V

# Redis Query Builder tests
ctest -R redis_query_builder_test -V

# All Query Builder tests
ctest -R query_builder -V
```

### 3.2 Verification Methods

**Query Output Verification**:
```cpp
// Check if generated query string matches expected
EXPECT_EQ(builder_.build(), expected_query);

// Check for specific keyword presence
EXPECT_TRUE(query.find("INNER JOIN") != std::string::npos);
```

**Real DB Execution Tests** (Integration):
```cpp
// Execute generated query on real DB to verify syntax correctness
auto result = db->execute_query(builder_.build());
EXPECT_TRUE(result.success());
```

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| SQL QB test case count | 27+ | ctest count |
| MongoDB QB test case count | 17+ | ctest count |
| Redis QB test case count | 18+ | ctest count |
| Query accuracy | 100% | String matching |
| Coverage (query_builder.cpp) | 85%+ | gcovr |

### 3.4 Edge Cases to Test

- Build query with empty conditions
- Special character handling in values
- SQL injection prevention verification (parameterized)
- Reuse after `reset()`
- Generate multiple queries with same builder

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| SQL dialect differences | HIGH | Separate verification per DB type |
| Missing complex query combinations | MEDIUM | Test based on actual usage patterns |
| MongoDB/Redis actual integration tests | LOW | Docker-based integration test environment |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**: [DB-012](DB-012-complexity.md) (query_builder.cpp Complexity)

---

## 6. Notes

- Document SQL injection prevention warnings for `*_raw()` methods
- Document thread safety warnings (builders are not thread-safe)
- Performance testing covered separately in [DB-007](DB-007-benchmark.md)

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
