# DB-012: Reduce query_builder.cpp Complexity

**Category**: REFACTOR
**Priority**: LOW
**Status**: IN_PROGRESS
**Est. Duration**: 5-7 days
**Dependencies**: None
**Assignee**: AI Assistant
**Created**: 2025-11-24
**Started**: 2025-11-25

## Progress Summary

### Completed (Phase 1):
- ✅ Extracted `value_formatter` class for value formatting and escaping
- ✅ Implemented `sql_dialect` classes (PostgreSQL, MySQL, SQLite) using Strategy pattern
- ✅ Extracted `condition_builder` class for WHERE clause construction
- ✅ Created `database/query_builder/` directory structure
- ✅ Updated CMakeLists.txt to build new components
- ✅ Written unit tests for value_formatter

### Remaining Work:
- Extract `join_builder` class
- Refactor `sql_builder` to use new helper classes
- Refactor `mongodb_builder` and `redis_builder`
- Update main `query_builder.cpp` as facade
- Complete test coverage
- Static analysis and complexity verification
- Performance regression testing

---

## 1. What to Change

### Current State
- `query_builder.cpp` contains multiple builder classes in a single file
- High cyclomatic complexity in some methods
- Duplicated logic across different query builder types
- String formatting logic mixed with query logic
- Limited extensibility for new database types

### Target State
- Split into smaller, focused files per builder type
- Reduce cyclomatic complexity to acceptable levels (< 10 per method)
- Extract common logic into shared utilities
- Clear separation between query construction and string formatting
- Template-based extensibility for new database types

### Scope
**Target Files**:
- `database/query_builder.h` - Interface definitions
- `database/query_builder.cpp` - Implementation

**New Files to Create**:
- `database/query_builder/sql_builder.cpp`
- `database/query_builder/mongodb_builder.cpp`
- `database/query_builder/redis_builder.cpp`
- `database/query_builder/value_formatter.cpp`
- `database/query_builder/query_utils.cpp`

---

## 2. How to Change

### 2.1 Current Code Analysis

**Identified Issues**:

```cpp
// Current: Large switch statements in format_value()
std::string sql_query_builder::format_value(const database_value& value,
                                            database_types db_type) const {
    // This method has high complexity with multiple nested conditions
    if (std::holds_alternative<std::string>(value)) {
        // String escaping logic mixed in
        std::string str = std::get<std::string>(value);
        // Multiple DB-specific branches...
    } else if (std::holds_alternative<int>(value)) {
        // ...
    } else if (std::holds_alternative<double>(value)) {
        // ...
    }
    // etc.
}

// Current: Duplicated identifier escaping across builders
std::string escape_identifier(const std::string& id, database_types db_type) {
    switch (db_type) {
        case database_types::PostgreSQL:
            return "\"" + id + "\"";
        case database_types::MySQL:
            return "`" + id + "`";
        // ... repeated in multiple places
    }
}
```

### 2.2 Proposed Refactoring

#### Extract Value Formatter

```cpp
// database/query_builder/value_formatter.h
#pragma once

#include "../database_types.h"
#include <string>
#include <variant>

namespace database::query {

/**
 * @brief Formats database values for different backends
 *
 * Single Responsibility: Handle value formatting and escaping
 */
class value_formatter {
public:
    explicit value_formatter(database_types db_type);

    std::string format(const database_value& value) const;
    std::string escape_string(const std::string& str) const;
    std::string escape_identifier(const std::string& identifier) const;
    std::string null_literal() const;

private:
    database_types db_type_;

    std::string format_string(const std::string& str) const;
    std::string format_number(int64_t num) const;
    std::string format_number(double num) const;
    std::string format_bool(bool val) const;
    std::string format_blob(const std::vector<uint8_t>& data) const;
};

} // namespace database::query
```

#### Split Builder Classes

```cpp
// database/query_builder/sql_builder.h
#pragma once

#include "builder_base.h"
#include "value_formatter.h"

namespace database::query {

/**
 * @brief SQL query builder for relational databases
 */
class sql_builder : public builder_base {
public:
    sql_builder();
    ~sql_builder() = default;

    // SELECT operations
    sql_builder& select(const column_list& columns);
    sql_builder& select_raw(const std::string& raw);
    sql_builder& from(const std::string& table);

    // WHERE conditions - delegated to condition_builder
    sql_builder& where(const condition& cond);
    sql_builder& where(const std::string& field, const std::string& op,
                       const database_value& value);
    sql_builder& or_where(const condition& cond);

    // JOIN operations - delegated to join_builder
    sql_builder& join(const join_spec& spec);
    sql_builder& left_join(const std::string& table, const std::string& on);
    sql_builder& right_join(const std::string& table, const std::string& on);

    // Build methods
    std::string build(database_types db_type) const override;

private:
    struct query_parts;
    std::unique_ptr<query_parts> parts_;

    std::string build_select(database_types db_type) const;
    std::string build_insert(database_types db_type) const;
    std::string build_update(database_types db_type) const;
    std::string build_delete(database_types db_type) const;
};

} // namespace database::query
```

#### Extract Condition Builder

```cpp
// database/query_builder/condition_builder.h
#pragma once

#include "../database_types.h"
#include "value_formatter.h"
#include <vector>

namespace database::query {

enum class logical_op { AND, OR };

struct condition {
    std::string field;
    std::string op;
    database_value value;
    std::string raw;  // For raw conditions

    bool is_raw() const { return !raw.empty(); }
};

/**
 * @brief Builds WHERE clause conditions
 *
 * Handles complex condition combinations with proper precedence
 */
class condition_builder {
public:
    condition_builder& add(const condition& cond, logical_op op = logical_op::AND);
    condition_builder& add_raw(const std::string& raw, logical_op op = logical_op::AND);

    // Grouping support
    condition_builder& begin_group();
    condition_builder& end_group();

    std::string build(const value_formatter& formatter) const;
    bool empty() const;
    void clear();

private:
    struct condition_node {
        condition cond;
        logical_op op;
        int group_level;
    };

    std::vector<condition_node> conditions_;
    int current_group_level_ = 0;
};

} // namespace database::query
```

#### Strategy Pattern for DB-Specific Logic

```cpp
// database/query_builder/dialect.h
#pragma once

#include <string>
#include <memory>

namespace database::query {

/**
 * @brief Abstract base for database-specific SQL dialects
 */
class sql_dialect {
public:
    virtual ~sql_dialect() = default;

    virtual std::string quote_identifier(const std::string& id) const = 0;
    virtual std::string quote_string(const std::string& str) const = 0;
    virtual std::string limit_clause(size_t limit, size_t offset) const = 0;
    virtual std::string bool_literal(bool val) const = 0;
    virtual std::string current_timestamp() const = 0;

    static std::unique_ptr<sql_dialect> create(database_types type);
};

/**
 * @brief PostgreSQL-specific dialect
 */
class postgresql_dialect : public sql_dialect {
public:
    std::string quote_identifier(const std::string& id) const override {
        return "\"" + id + "\"";
    }

    std::string quote_string(const std::string& str) const override {
        // PostgreSQL escape sequences
        std::string result = "'";
        for (char c : str) {
            if (c == '\'') result += "''";
            else result += c;
        }
        return result + "'";
    }

    std::string limit_clause(size_t limit, size_t offset) const override {
        std::string clause = "LIMIT " + std::to_string(limit);
        if (offset > 0) clause += " OFFSET " + std::to_string(offset);
        return clause;
    }

    std::string bool_literal(bool val) const override {
        return val ? "TRUE" : "FALSE";
    }

    std::string current_timestamp() const override {
        return "CURRENT_TIMESTAMP";
    }
};

/**
 * @brief MySQL-specific dialect
 */
class mysql_dialect : public sql_dialect {
public:
    std::string quote_identifier(const std::string& id) const override {
        return "`" + id + "`";
    }

    std::string quote_string(const std::string& str) const override {
        // MySQL escape sequences
        std::string result = "'";
        for (char c : str) {
            switch (c) {
                case '\'': result += "\\'"; break;
                case '\\': result += "\\\\"; break;
                default: result += c;
            }
        }
        return result + "'";
    }

    std::string limit_clause(size_t limit, size_t offset) const override {
        if (offset > 0) {
            return "LIMIT " + std::to_string(offset) + ", " + std::to_string(limit);
        }
        return "LIMIT " + std::to_string(limit);
    }

    std::string bool_literal(bool val) const override {
        return val ? "1" : "0";
    }

    std::string current_timestamp() const override {
        return "NOW()";
    }
};

} // namespace database::query
```

### 2.3 File Structure After Refactoring

```
database/
├── query_builder.h              # Public API (unchanged interface)
├── query_builder.cpp            # Facade implementation
└── query_builder/
    ├── builder_base.h           # Abstract builder base
    ├── sql_builder.h/cpp        # SQL-specific builder
    ├── mongodb_builder.h/cpp    # MongoDB-specific builder
    ├── redis_builder.h/cpp      # Redis-specific builder
    ├── condition_builder.h/cpp  # WHERE clause builder
    ├── join_builder.h/cpp       # JOIN clause builder
    ├── value_formatter.h/cpp    # Value formatting utilities
    ├── dialect.h/cpp            # DB-specific dialects
    └── query_utils.h/cpp        # Shared utilities
```

### 2.4 Complexity Metrics

**Target Complexity Limits**:
| Metric | Current | Target |
|--------|---------|--------|
| Max cyclomatic complexity | 25+ | < 10 |
| Max lines per method | 100+ | < 30 |
| Max file length | 1500+ | < 500 |
| Methods per class | 40+ | < 15 |

### 2.5 Implementation Steps

1. **Extract Value Formatter** (Day 1)
   - Create value_formatter class
   - Move formatting logic
   - Add unit tests

2. **Extract Dialect Classes** (Day 2)
   - Create sql_dialect base
   - Implement PostgreSQL/MySQL/SQLite dialects
   - Add unit tests

3. **Split Condition Builder** (Day 3)
   - Extract condition_builder class
   - Implement grouping support
   - Add unit tests

4. **Split SQL Builder** (Day 4)
   - Create sql_builder with cleaner interface
   - Delegate to helper classes
   - Add unit tests

5. **Refactor MongoDB/Redis Builders** (Day 5)
   - Apply similar patterns
   - Extract common utilities
   - Add unit tests

6. **Integration and Cleanup** (Days 6-7)
   - Update main query_builder.cpp as facade
   - Ensure backward compatibility
   - Performance testing
   - Documentation update

---

## 3. How to Test

### 3.1 Unit Tests

```cpp
// tests/query_builder/value_formatter_test.cpp
TEST(ValueFormatterTest, PostgreSQLStringEscaping) {
    value_formatter fmt(database_types::PostgreSQL);
    EXPECT_EQ(fmt.escape_string("O'Brien"), "O''Brien");
    EXPECT_EQ(fmt.format(std::string("test")), "'test'");
}

TEST(ValueFormatterTest, MySQLStringEscaping) {
    value_formatter fmt(database_types::MySQL);
    EXPECT_EQ(fmt.escape_string("O'Brien"), "O\\'Brien");
}

// tests/query_builder/condition_builder_test.cpp
TEST(ConditionBuilderTest, ComplexConditions) {
    condition_builder builder;
    builder.add({"status", "=", std::string("active")})
           .begin_group()
           .add({"age", ">", 18}, logical_op::AND)
           .add({"age", "<", 65}, logical_op::AND)
           .end_group();

    value_formatter fmt(database_types::PostgreSQL);
    auto sql = builder.build(fmt);

    EXPECT_EQ(sql, "status = 'active' AND (age > 18 AND age < 65)");
}

// tests/query_builder/dialect_test.cpp
TEST(DialectTest, PostgreSQLIdentifiers) {
    auto dialect = sql_dialect::create(database_types::PostgreSQL);
    EXPECT_EQ(dialect->quote_identifier("table"), "\"table\"");
}

TEST(DialectTest, MySQLIdentifiers) {
    auto dialect = sql_dialect::create(database_types::MySQL);
    EXPECT_EQ(dialect->quote_identifier("table"), "`table`");
}
```

### 3.2 Backward Compatibility Tests

```cpp
// tests/query_builder/compatibility_test.cpp
TEST(CompatibilityTest, ExistingAPIUnchanged) {
    // All existing tests should pass without modification
    sql_query_builder builder;

    auto query = builder
        .select({"id", "name"})
        .from("users")
        .where("active", "=", true)
        .limit(10)
        .build();

    EXPECT_TRUE(query.find("SELECT id, name") != std::string::npos);
}
```

### 3.3 Performance Tests

```cpp
// Ensure no performance regression
TEST(PerformanceTest, BuilderOverhead) {
    constexpr int ITERATIONS = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        sql_query_builder builder;
        auto query = builder
            .select({"*"})
            .from("users")
            .where("id", "=", i)
            .build();
        benchmark::DoNotOptimize(query);
    }

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ns_per_op = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / ITERATIONS;

    // Should be < 1000ns per simple query build
    EXPECT_LT(ns_per_op, 1000);
}
```

### 3.4 Static Analysis

```bash
# Run cppcheck for complexity
cppcheck --enable=all --inconclusive \
  --suppress=missingIncludeSystem \
  database/query_builder/

# Check cyclomatic complexity
lizard database/query_builder/ -C 10 -w
```

### 3.5 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Cyclomatic complexity | < 10 per method | lizard analysis |
| Lines per file | < 500 | wc -l |
| All existing tests pass | 100% | ctest |
| Performance regression | < 5% | Benchmark |
| New unit tests | 30+ | Test count |

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Breaking changes | HIGH | Comprehensive backward compatibility tests |
| Performance regression | MEDIUM | Benchmark before/after |
| Incomplete refactoring | LOW | Incremental approach with tests |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-002](DB-002-orm-tests.md) (ORM Tests - tests the refactored code)

---

## 6. Notes

- Maintain existing public API for backward compatibility
- Internal restructuring should be transparent to users
- Consider using PIMPL pattern for further ABI stability
- Document new extension points for adding database types

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
