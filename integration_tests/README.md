# Database System Integration Tests

Comprehensive integration testing suite for database_system, providing end-to-end testing across connection management, query execution, performance, and error handling scenarios.

## Overview

This integration test suite validates the database_system under realistic conditions with:
- **45+ integration tests** across 4 test suites
- Connection pool behavior and lifecycle management
- Query execution with transactions and concurrency
- Performance benchmarking against defined baselines
- Error handling and failure recovery scenarios

## Test Structure

### Test Suites

#### 1. Connection Management Tests (`scenarios/connection_management_test.cpp`)
**15 tests** covering connection pool operations:

- Pool initialization (default and custom configurations)
- Connection acquisition and release
- Connection pooling and reuse patterns
- Timeout handling and max connections enforcement
- Health checking and metadata tracking
- Concurrent connection requests
- Idle timeout detection
- Pool statistics and shutdown behavior

#### 2. Query Execution Tests (`scenarios/query_execution_test.cpp`)
**13 tests** covering database operations:

- Simple CRUD operations (SELECT, INSERT, UPDATE, DELETE)
- Prepared statement patterns
- Transaction management (BEGIN, COMMIT, ROLLBACK)
- Batch operations
- Parameterized queries
- Result set iteration and access
- WHERE clause filtering and ORDER BY
- Concurrent query execution

#### 3. Performance Tests (`performance/database_performance_test.cpp`)
**9 tests** measuring system performance:

- Connection pool throughput (target: >1000 ops/sec)
- Query execution latency (P50, P95, P99)
- Connection acquisition latency (target: <1ms)
- Batch insert performance (1000 rows in <100ms)
- Transaction commit latency (target: <20ms)
- Connection pool scalability under load
- Memory usage tracking
- Concurrent query throughput
- Prepared statement performance

#### 4. Error Handling Tests (`failures/error_handling_test.cpp`)
**12 tests** validating error scenarios:

- Invalid query syntax handling
- Non-existent table access
- Constraint violations (PRIMARY KEY, UNIQUE, NOT NULL)
- Transaction rollback on errors
- Connection pool exhaustion
- Invalid database files
- Disconnected database operations
- Invalid connection strings
- Concurrent constraint violations
- Recovery from unhealthy connections

## Performance Baselines

The following performance baselines are validated in Release builds:

| Metric | Target | Test |
|--------|--------|------|
| Connection pool throughput | > 1000 ops/sec | `ConnectionPoolThroughput` |
| Query latency P50 | < 10ms | `QueryExecutionLatency` |
| Query latency P95 | < 50ms | `QueryExecutionLatency` |
| Query latency P99 | < 100ms | `QueryExecutionLatency` |
| Connection acquisition P50 | < 1ms | `ConnectionAcquisitionLatency` |
| Batch insert (1000 rows) | < 100ms | `BatchInsertPerformance` |
| Transaction commit P50 | < 20ms | `TransactionCommitLatency` |

## Running Tests

### Quick Start

```bash
# Configure with integration tests enabled
cmake -B build -DDATABASE_BUILD_INTEGRATION_TESTS=ON -DUSE_SQLITE=ON

# Build integration tests
cmake --build build --target database_integration_tests

# Run all integration tests
./build/bin/database_integration_tests
```

### Running Specific Test Suites

```bash
# Connection management tests only
./build/bin/database_integration_tests --gtest_filter=ConnectionManagement*

# Query execution tests only
./build/bin/database_integration_tests --gtest_filter=QueryExecution*

# Performance tests only
./build/bin/database_integration_tests --gtest_filter=*Performance*

# Error handling tests only
./build/bin/database_integration_tests --gtest_filter=ErrorHandling*
```

### CMake Targets

```bash
# Run integration tests via CMake
cmake --build build --target run_integration_tests

# Generate coverage report (Debug builds only)
cmake --build build --target integration_coverage
```

## Test Framework

### Fixtures

**DatabaseSystemFixture** (`framework/system_fixture.h`)
- Base fixture for all database tests
- Automatic SQLite database setup and teardown
- Test table creation and data insertion helpers
- Query execution wrappers

**ConnectionPoolFixture** (`framework/system_fixture.h`)
- Extends `DatabaseSystemFixture` with connection pool support
- Pre-configured connection pool for testing
- Pool statistics and health check utilities

### Helper Utilities

**Performance Measurement** (`framework/test_helpers.h`)
- `PerformanceTimer`: High-resolution timing
- `LatencyTracker`: Statistical analysis (P50, P95, P99)
- `MeasureThroughput`: Operation throughput measurement

**Test Helpers** (`framework/test_helpers.h`)
- `TransactionHelper`: Transaction lifecycle management
- `GenerateRandomString`: Test data generation
- `VerifyData`: Result set validation
- `WaitFor`: Condition-based waiting with timeout

## Coverage Goals

- **Target coverage**: 80% for database module
- **Critical paths**: 100% coverage for connection pool and query execution
- **Error paths**: Complete coverage of all error handling scenarios

## CI/CD Integration

Integration tests run automatically on:
- Push to `main`, `develop`, or `feat/*` branches
- Pull requests to `main` or `develop`
- Manual workflow dispatch

### Matrix Testing

Tests run on:
- **Operating Systems**: Ubuntu Latest, macOS Latest
- **Build Types**: Debug, Release
- **Compilers**: GCC 11 (Ubuntu), Clang (macOS)

### Artifacts

- Test results (XML format)
- Coverage reports (Debug builds)
- Performance baseline validation (Release builds)

## Test Database

Tests use **SQLite** for:
- No external database dependencies
- Fast test execution
- Isolated test environments
- Easy CI/CD integration

Each test creates a unique temporary database file that is automatically cleaned up after the test completes.

## Adding New Tests

### 1. Choose the Appropriate Suite

- **Connection management**: Connection pool behavior
- **Query execution**: SQL operations and transactions
- **Performance**: Benchmarking and latency measurements
- **Error handling**: Failure scenarios and recovery

### 2. Use Appropriate Fixture

```cpp
class MyTest : public DatabaseSystemFixture {
    // For basic database operations
};

class MyTest : public ConnectionPoolFixture {
    // For connection pool testing
};
```

### 3. Follow Naming Conventions

```cpp
TEST_F(MyTest, DescriptiveTestName) {
    // Arrange
    InsertTestUsers(10);

    // Act
    auto result = ExecuteQuery("SELECT * FROM users");

    // Assert
    EXPECT_EQ(result.size(), 10u);
}
```

### 4. Update Documentation

- Add test description to this README
- Update test count in summary
- Document any new performance baselines

## Troubleshooting

### Common Issues

**SQLite not found**
```bash
# Ubuntu
sudo apt-get install libsqlite3-dev

# macOS
brew install sqlite3
```

**GTest not found**
```bash
# Ubuntu
sudo apt-get install libgtest-dev

# macOS
brew install googletest
```

**Coverage tools not available**
```bash
# Ubuntu/macOS
sudo apt-get install lcov  # Ubuntu
brew install lcov          # macOS
```

### Debug Failed Tests

```bash
# Run with verbose output
./build/bin/database_integration_tests --gtest_filter=FailedTest* --gtest_repeat=1 --gtest_break_on_failure

# Run specific test with debug info
gdb --args ./build/bin/database_integration_tests --gtest_filter=FailedTest*
```

## Contributing

When adding new integration tests:

1. Ensure tests are isolated and don't depend on execution order
2. Clean up resources in fixture `TearDown()` methods
3. Use meaningful test names that describe what is being tested
4. Include both positive and negative test cases
5. Document any new performance baselines
6. Update test counts in this README

## References

- [Google Test Documentation](https://google.github.io/googletest/)
- [SQLite Documentation](https://www.sqlite.org/docs.html)
- [database_system API Documentation](../docs/)
- [Performance Tuning Guide](../docs/performance.md)
