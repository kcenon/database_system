# DB-014: Add Mock Object Framework

**Category**: REFACTOR
**Priority**: LOW
**Status**: TODO
**Est. Duration**: 3-4 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Tests require actual database connections
- Integration tests are slow due to real database operations
- Difficult to test edge cases (network failures, timeouts)
- No mock implementations for database interfaces
- Test isolation is challenging

### Target State
- Mock implementations for all database interfaces
- Fast unit tests without database dependencies
- Easy simulation of error conditions
- Configurable mock behaviors for different test scenarios
- Recording of all database operations for verification

### Scope
**Target Interfaces**:
- `database_base` - Core database interface
- `database_result` - Query result type
- `connection_pool` - Connection pooling
- `database_gateway` - Gateway interface
- `replication_manager` - Replication interface

**New Files to Create**:
- `tests/mocks/mock_database.h/cpp`
- `tests/mocks/mock_connection_pool.h/cpp`
- `tests/mocks/mock_gateway.h/cpp`
- `tests/mocks/mock_expectations.h/cpp`
- `tests/mocks/database_test_utils.h/cpp`

---

## 2. How to Change

### 2.1 Mock Database Implementation

```cpp
// tests/mocks/mock_database.h
#pragma once

#include "database/database_base.h"
#include "mock_expectations.h"
#include <queue>
#include <functional>

namespace database::testing {

/**
 * @brief Configurable mock for database_base interface
 *
 * Allows setting expected queries and their results,
 * simulating errors, and verifying database operations.
 */
class mock_database : public database_base {
public:
    mock_database();
    ~mock_database() override;

    // database_base interface implementation
    database_types database_type() override;
    bool connect(const std::string& connection_string) override;
    bool disconnect() override;
    bool create_query(const std::string& query) override;
    unsigned int insert_query(const std::string& query) override;
    unsigned int update_query(const std::string& query) override;
    unsigned int delete_query(const std::string& query) override;
    database_result select_query(const std::string& query) override;
    bool execute_query(const std::string& query) override;

    // Mock configuration
    mock_database& set_database_type(database_types type);
    mock_database& set_connect_result(bool result);
    mock_database& set_default_select_result(const database_result& result);

    // Expectation setting
    mock_database& expect_query(const std::string& query);
    mock_database& will_return(const database_result& result);
    mock_database& will_return_rows(unsigned int count);
    mock_database& will_fail(const std::string& error_message);
    mock_database& will_throw(const std::exception& ex);
    mock_database& times(int count);
    mock_database& any_times();

    // Query matching options
    mock_database& matching_exactly(const std::string& query);
    mock_database& matching_pattern(const std::string& regex_pattern);
    mock_database& matching_any();

    // Error simulation
    mock_database& simulate_connection_failure();
    mock_database& simulate_timeout(std::chrono::milliseconds delay);
    mock_database& simulate_disconnect_during_query();

    // Verification
    bool verify_all_expectations() const;
    std::vector<std::string> get_executed_queries() const;
    int get_query_count(const std::string& pattern = "") const;
    void reset();

private:
    struct impl;
    std::unique_ptr<impl> impl_;

    void record_query(const std::string& query);
    expectation* find_expectation(const std::string& query);
};

/**
 * @brief Builder for common mock configurations
 */
class mock_database_builder {
public:
    mock_database_builder();

    // Preset configurations
    static mock_database empty_database();
    static mock_database with_users_table(const std::vector<user_data>& users);
    static mock_database failing_database(const std::string& error);
    static mock_database slow_database(std::chrono::milliseconds latency);

    // Fluent configuration
    mock_database_builder& with_table(const std::string& name,
                                      const database_result& data);
    mock_database_builder& that_fails_on(const std::string& query_pattern);
    mock_database_builder& with_latency(std::chrono::milliseconds ms);

    mock_database build();

private:
    std::unique_ptr<mock_database> mock_;
};

} // namespace database::testing
```

### 2.2 Expectation Framework

```cpp
// tests/mocks/mock_expectations.h
#pragma once

#include "database/database_types.h"
#include <string>
#include <functional>
#include <regex>
#include <optional>

namespace database::testing {

enum class match_type {
    EXACT,
    PATTERN,
    ANY
};

/**
 * @brief Single query expectation with configurable behavior
 */
class expectation {
public:
    expectation() = default;

    // Query matching
    expectation& for_query(const std::string& query, match_type type = match_type::EXACT);

    // Response configuration
    expectation& returning(const database_result& result);
    expectation& returning_rows(unsigned int count);
    expectation& throwing(std::exception_ptr ex);
    expectation& failing_with(const std::string& error);

    // Invocation limits
    expectation& times(int count);
    expectation& at_least(int count);
    expectation& at_most(int count);
    expectation& never();

    // Side effects
    expectation& then(std::function<void()> callback);
    expectation& with_delay(std::chrono::milliseconds delay);

    // Matching
    bool matches(const std::string& query) const;
    bool is_satisfied() const;
    bool can_be_invoked() const;

    // Invocation
    database_result invoke();
    unsigned int invoke_modification();

private:
    std::string query_;
    match_type match_type_ = match_type::EXACT;
    std::regex pattern_;

    std::optional<database_result> result_;
    std::optional<unsigned int> row_count_;
    std::optional<std::string> error_;
    std::exception_ptr exception_;

    int min_invocations_ = 1;
    int max_invocations_ = 1;
    int actual_invocations_ = 0;

    std::function<void()> callback_;
    std::chrono::milliseconds delay_{0};
};

/**
 * @brief Manages a set of expectations with priority ordering
 */
class expectation_set {
public:
    void add(expectation exp);
    expectation* find_match(const std::string& query);
    bool all_satisfied() const;
    std::vector<std::string> unsatisfied_expectations() const;
    void clear();

private:
    std::vector<expectation> expectations_;
};

} // namespace database::testing
```

### 2.3 Test Utilities

```cpp
// tests/mocks/database_test_utils.h
#pragma once

#include "mock_database.h"
#include <gtest/gtest.h>

namespace database::testing {

/**
 * @brief Test fixture with mock database support
 */
class MockDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Access mock
    mock_database& db() { return *mock_db_; }

    // Assertion helpers
    void expect_no_database_calls();
    void expect_single_query(const std::string& query);

    // Common setup
    void setup_empty_database();
    void setup_users_table(int user_count = 10);

    // Verification
    void verify_query_executed(const std::string& pattern);
    void verify_query_not_executed(const std::string& pattern);
    void verify_query_count(const std::string& pattern, int expected);

private:
    std::unique_ptr<mock_database> mock_db_;
};

/**
 * @brief Helper to create test data
 */
class test_data_builder {
public:
    static database_result users(int count);
    static database_result products(int count);
    static database_result orders(int count);

    static database_result custom_table(
        const std::vector<std::string>& columns,
        const std::vector<std::vector<database_value>>& rows
    );
};

/**
 * @brief Matchers for query verification
 */
namespace matchers {
    bool query_contains(const std::string& query, const std::string& substring);
    bool query_starts_with(const std::string& query, const std::string& prefix);
    bool query_matches_pattern(const std::string& query, const std::string& regex);
    bool is_select_query(const std::string& query);
    bool is_insert_query(const std::string& query);
    bool is_update_query(const std::string& query);
    bool is_delete_query(const std::string& query);
}

} // namespace database::testing
```

### 2.4 Mock Connection Pool

```cpp
// tests/mocks/mock_connection_pool.h
#pragma once

#include "database/connection_pool.h"
#include "mock_database.h"

namespace database::testing {

/**
 * @brief Mock connection pool for testing pooling behavior
 */
class mock_connection_pool {
public:
    explicit mock_connection_pool(const pool_config& config = {});

    // Pool interface
    mock_database* acquire();
    void release(mock_database* conn);
    bool try_acquire(std::chrono::milliseconds timeout, mock_database*& conn);

    // Mock configuration
    void set_pool_exhausted(bool exhausted);
    void set_acquire_delay(std::chrono::milliseconds delay);
    void set_max_connections(size_t max);

    // Verification
    size_t active_count() const;
    size_t idle_count() const;
    size_t total_acquisitions() const;
    size_t total_releases() const;

    // Error simulation
    void simulate_acquire_failure();
    void simulate_leak_detection();

private:
    pool_config config_;
    std::vector<std::unique_ptr<mock_database>> connections_;
    std::queue<mock_database*> available_;
    std::set<mock_database*> in_use_;
    std::mutex mutex_;

    bool exhausted_ = false;
    std::chrono::milliseconds acquire_delay_{0};
    size_t acquisition_count_ = 0;
    size_t release_count_ = 0;
};

} // namespace database::testing
```

### 2.5 Example Usage

```cpp
// tests/service_test.cpp
#include "mocks/mock_database.h"
#include "mocks/database_test_utils.h"
#include "services/user_service.h"

using namespace database::testing;

class UserServiceTest : public MockDatabaseTest {
protected:
    void SetUp() override {
        MockDatabaseTest::SetUp();
        service_ = std::make_unique<UserService>(&db());
    }

    std::unique_ptr<UserService> service_;
};

TEST_F(UserServiceTest, GetUserById_ReturnsUser) {
    // Setup expectation
    db().expect_query("SELECT * FROM users WHERE id = 123")
        .will_return(test_data_builder::custom_table(
            {"id", "name", "email"},
            {{123, "John", "john@test.com"}}
        ));

    // Execute
    auto user = service_->get_user_by_id(123);

    // Verify
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->name, "John");
    EXPECT_EQ(user->email, "john@test.com");
    EXPECT_TRUE(db().verify_all_expectations());
}

TEST_F(UserServiceTest, GetUserById_NotFound) {
    // Setup: Return empty result
    db().expect_query(matching_pattern("SELECT .* FROM users WHERE id = .*"))
        .will_return(database_result{});

    // Execute
    auto user = service_->get_user_by_id(999);

    // Verify
    EXPECT_FALSE(user.has_value());
}

TEST_F(UserServiceTest, CreateUser_HandlesDatabaseError) {
    // Setup: Simulate database failure
    db().expect_query(matching_pattern("INSERT INTO users.*"))
        .will_fail("Duplicate key violation");

    // Execute & Verify
    EXPECT_THROW(
        service_->create_user("existing@test.com", "Test"),
        DatabaseException
    );
}

TEST_F(UserServiceTest, BulkOperation_TransactionHandling) {
    // Setup: Expect transaction boundaries
    db().expect_query("BEGIN").times(1);
    db().expect_query(matching_pattern("INSERT.*")).times(3);
    db().expect_query("COMMIT").times(1);

    // Execute
    service_->bulk_create_users({
        {"user1@test.com", "User1"},
        {"user2@test.com", "User2"},
        {"user3@test.com", "User3"},
    });

    // Verify all expectations met
    EXPECT_TRUE(db().verify_all_expectations());
}
```

### 2.6 Implementation Steps

1. **Core Mock Database** (Day 1)
   - Implement mock_database class
   - Basic expectation matching
   - Query recording

2. **Expectation Framework** (Day 2)
   - Implement expectation class
   - Pattern matching support
   - Invocation counting

3. **Test Utilities** (Day 2-3)
   - Test fixture base class
   - Data builders
   - Query matchers

4. **Mock Connection Pool** (Day 3)
   - Pool simulation
   - Exhaustion scenarios
   - Leak detection

5. **Integration & Examples** (Day 4)
   - Convert existing tests to use mocks
   - Add example tests
   - Documentation

---

## 3. How to Test

### 3.1 Mock Framework Tests

```cpp
// tests/mocks/mock_database_test.cpp
TEST(MockDatabaseTest, BasicExpectation) {
    mock_database db;

    db.expect_query("SELECT 1")
      .will_return({{{"result", 1}}});

    auto result = db.select_query("SELECT 1");
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(db.verify_all_expectations());
}

TEST(MockDatabaseTest, UnmetExpectation) {
    mock_database db;
    db.expect_query("SELECT 1");

    // Don't execute query
    EXPECT_FALSE(db.verify_all_expectations());
}

TEST(MockDatabaseTest, PatternMatching) {
    mock_database db;

    db.expect_query(matching_pattern("SELECT .* FROM users"))
      .will_return(test_data_builder::users(5));

    auto result = db.select_query("SELECT id, name FROM users");
    EXPECT_EQ(result.size(), 5);
}

TEST(MockDatabaseTest, ErrorSimulation) {
    mock_database db;

    db.expect_query("INSERT INTO users")
      .will_fail("Connection lost");

    EXPECT_THROW(db.insert_query("INSERT INTO users"), DatabaseException);
}
```

### 3.2 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Mock all database interfaces | 100% | Interface coverage |
| Expectation framework tests | 20+ | Test count |
| Example tests converted | 10+ | Test count |
| Documentation | Complete | Review |

### 3.3 Performance Comparison

```cpp
// Demonstrate speed improvement with mocks
TEST(PerformanceTest, MockVsRealDatabase) {
    // Mock version
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        mock_database db;
        db.expect_query(matching_any()).will_return({});
        db.select_query("SELECT 1");
    }
    auto mock_time = std::chrono::high_resolution_clock::now() - start;

    // Real SQLite version
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        sqlite_manager db;
        db.connect(":memory:");
        db.select_query("SELECT 1");
        db.disconnect();
    }
    auto real_time = std::chrono::high_resolution_clock::now() - start;

    // Mock should be significantly faster
    EXPECT_LT(mock_time, real_time / 10);
}
```

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Mock behavior diverges from real | HIGH | Periodic validation tests |
| Over-mocking hides bugs | MEDIUM | Keep integration tests |
| Complex mock setup | LOW | Builder patterns, presets |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-001](DB-001-backend-tests.md) (Backend Tests)
  - [DB-003](DB-003-resilience-tests.md) (Resilience Tests)

---

## 6. Notes

- Mocks are for unit tests; keep integration tests with real databases
- Consider using GoogleMock for additional matchers
- Document common mock patterns in CONTRIBUTING.md
- Validate mock behavior against real databases periodically

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
