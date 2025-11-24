# DB-001: Complete MySQL & SQLite Backend Tests

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
- MySQL and SQLite backends are implemented but lack adequate test coverage
- Current test coverage: ~65%
- Only basic CRUD tests exist; edge cases and error handling tests are missing

### Target State
- Complete tests for all public APIs of MySQL/SQLite backends
- Achieve test coverage of 80% or higher
- Add tests for error handling, boundary conditions, and concurrency scenarios

### Scope
**Target Files**:
- `database/backends/mysql/mysql_manager.h`
- `database/backends/mysql/mysql_manager.cpp`
- `database/backends/sqlite/sqlite_manager.h`
- `database/backends/sqlite/sqlite_manager.cpp`

**Test Files to Add/Modify**:
- `tests/mysql_manager_test.cpp` (new or extended)
- `tests/sqlite_manager_test.cpp` (new or extended)

---

## 2. How to Change

### 2.1 MySQL Manager Tests

```cpp
// tests/mysql_manager_test.cpp
#include <gtest/gtest.h>
#include "database/backends/mysql/mysql_manager.h"

class MySQLManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<database::mysql_manager>();
    }

    void TearDown() override {
        if (manager_) {
            manager_->disconnect();
        }
    }

    std::unique_ptr<database::mysql_manager> manager_;
};

// Connection Tests
TEST_F(MySQLManagerTest, ConnectWithValidCredentials) {
    bool result = manager_->connect(
        "host=localhost;port=3306;database=test;user=test;password=test"
    );
    EXPECT_TRUE(result);
}

TEST_F(MySQLManagerTest, ConnectWithInvalidCredentials) {
    bool result = manager_->connect(
        "host=localhost;port=3306;database=test;user=invalid;password=wrong"
    );
    EXPECT_FALSE(result);
}

TEST_F(MySQLManagerTest, ConnectWithMalformedString) {
    bool result = manager_->connect("invalid_connection_string");
    EXPECT_FALSE(result);
}

// CRUD Tests
TEST_F(MySQLManagerTest, InsertAndSelectQuery) { /* ... */ }
TEST_F(MySQLManagerTest, UpdateQuery) { /* ... */ }
TEST_F(MySQLManagerTest, DeleteQuery) { /* ... */ }

// Error Handling Tests
TEST_F(MySQLManagerTest, QueryOnDisconnectedDatabase) { /* ... */ }
TEST_F(MySQLManagerTest, InvalidSQLSyntax) { /* ... */ }
TEST_F(MySQLManagerTest, DuplicateKeyError) { /* ... */ }

// Thread Safety Tests (if applicable)
TEST_F(MySQLManagerTest, ConcurrentQueries) { /* ... */ }
```

### 2.2 SQLite Manager Tests

```cpp
// tests/sqlite_manager_test.cpp
#include <gtest/gtest.h>
#include "database/backends/sqlite/sqlite_manager.h"

class SQLiteManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<database::sqlite_manager>();
    }

    void TearDown() override {
        if (manager_) {
            manager_->disconnect();
        }
        std::remove(test_db_path_.c_str());
    }

    std::unique_ptr<database::sqlite_manager> manager_;
    std::string test_db_path_ = "test_database.db";
};

// Connection Tests
TEST_F(SQLiteManagerTest, ConnectToFileDatabase) {
    EXPECT_TRUE(manager_->connect(test_db_path_));
}

TEST_F(SQLiteManagerTest, ConnectToMemoryDatabase) {
    EXPECT_TRUE(manager_->connect(":memory:"));
}

TEST_F(SQLiteManagerTest, ConnectToInvalidPath) {
    EXPECT_FALSE(manager_->connect("/nonexistent/path/db.sqlite"));
}

// Thread Safety Tests (recursive_mutex)
TEST_F(SQLiteManagerTest, ConcurrentReadWrites) {
    // Test that recursive_mutex prevents data corruption
}
```

### 2.3 Implementation Steps

1. **Environment Setup** (Day 1)
   - Set up Docker-based MySQL test environment
   - Create SQLite temporary file management utilities
   - Configure automatic test DB provisioning in CI/CD

2. **MySQL Test Implementation** (Days 2-3)
   - Connection/disconnection tests (5 tests)
   - CRUD operation tests (10 tests)
   - Error handling tests (8 tests)
   - Transaction tests (5 tests)

3. **SQLite Test Implementation** (Days 4-5)
   - Connection/disconnection tests (4 tests)
   - CRUD operation tests (10 tests)
   - Concurrency tests (recursive_mutex verification) (5 tests)
   - File handling tests (3 tests)

4. **Integration and Coverage Verification** (Days 6-7)
   - Run complete test suite
   - Generate and analyze coverage reports
   - Fill gaps in branch/edge case coverage

---

## 3. How to Test

### 3.1 Unit Test Execution

```bash
# Run MySQL tests
cd build
ctest -R mysql_manager_test -V

# Run SQLite tests
ctest -R sqlite_manager_test -V

# Run all backend tests
ctest -R "mysql|sqlite" -V
```

### 3.2 Test Environment Requirements

**MySQL**:
```yaml
# docker-compose.test.yml
services:
  mysql-test:
    image: mysql:8.0
    environment:
      MYSQL_ROOT_PASSWORD: test
      MYSQL_DATABASE: test
      MYSQL_USER: test
      MYSQL_PASSWORD: test
    ports:
      - "3306:3306"
```

**SQLite**: No separate environment needed (use in-memory mode)

### 3.3 Coverage Analysis

```bash
# Build with coverage enabled
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build

# Run tests and generate coverage report
cd build
ctest -R "mysql|sqlite"
gcovr --root .. --filter "../database/backends" --html coverage.html
```

### 3.4 Acceptance Criteria

| Criteria | Target | Verification Method |
|----------|--------|---------------------|
| MySQL test case count | 28+ | `ctest -N -R mysql` |
| SQLite test case count | 22+ | `ctest -N -R sqlite` |
| Line coverage | 80%+ | gcovr report |
| Branch coverage | 70%+ | gcovr report |
| All tests passing | 100% | `ctest --output-on-failure` |

### 3.5 Regression Testing

- Existing PostgreSQL tests should not be affected
- Integration tests (`integration_tests/`) must pass
- Sample code (`samples/`) must work correctly

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| MySQL connection failures (CI) | HIGH | Standardize Docker-based test environment |
| SQLite file locking issues | MEDIUM | Isolate temporary files per test |
| Increased test execution time | LOW | Enable parallel test execution |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**: [DB-006](DB-006-coverage.md) (Coverage Threshold)

---

## 6. Notes

- Test based on MySQL 8.0 or higher
- Verify SQLite 3.x latest version compatibility
- Ensure thorough test data cleanup

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
