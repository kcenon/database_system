# DB-003: Resilience Module Integration Tests

**Category**: TEST
**Priority**: HIGH
**Status**: DONE
**Est. Duration**: 4-5 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change

### Current State
- Resilience features like connection pooling and reconnection logic are implemented
- Utilities like `connection_leak_detector.h` exist
- Tests for scenarios like network failures and DB server restarts are missing
- Connection pool behavior verification tests are lacking

### Target State
- Implement network failure simulation tests
- Add connection pool boundary condition tests
- Verify connection leak detection functionality
- Test auto-reconnection and recovery scenarios

### Scope
**Target Features**:
- Connection Pool Management
- Connection Leak Detection
- Auto-reconnection Logic
- Failover Handling

**Test Files to Add/Modify**:
- `tests/connection_pool_test.cpp`
- `tests/connection_leak_detector_test.cpp`
- `tests/resilience_integration_test.cpp`

---

## 2. How to Change

### 2.1 Connection Pool Tests

```cpp
// tests/connection_pool_test.cpp
#include <gtest/gtest.h>
#include "database/connection_pool.h"
#include <thread>
#include <vector>

class ConnectionPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.min_connections = 2;
        config_.max_connections = 10;
        config_.idle_timeout_ms = 30000;
        config_.connection_string = ":memory:"; // SQLite
    }

    pool_config config_;
};

// Basic Pool Operations
TEST_F(ConnectionPoolTest, AcquireAndRelease) {
    ConnectionPool pool(config_);
    pool.initialize();

    auto conn = pool.acquire();
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(pool.active_connections(), 1);

    pool.release(conn);
    EXPECT_EQ(pool.active_connections(), 0);
}

// Pool Exhaustion
TEST_F(ConnectionPoolTest, PoolExhaustion) {
    config_.max_connections = 2;
    ConnectionPool pool(config_);
    pool.initialize();

    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();

    // Third acquisition should block or fail
    auto conn3 = pool.try_acquire(std::chrono::milliseconds(100));
    EXPECT_EQ(conn3, nullptr);
}

// Concurrent Access
TEST_F(ConnectionPoolTest, ConcurrentAcquisition) {
    config_.max_connections = 5;
    ConnectionPool pool(config_);
    pool.initialize();

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool, &success_count]() {
            auto conn = pool.acquire();
            if (conn) {
                success_count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                pool.release(conn);
            }
        });
    }

    for (auto& t : threads) t.join();

    // All threads should eventually succeed due to release
    EXPECT_GE(success_count, 5);
}

// Idle Connection Cleanup
TEST_F(ConnectionPoolTest, IdleConnectionEviction) {
    config_.idle_timeout_ms = 100; // Short timeout for test
    ConnectionPool pool(config_);
    pool.initialize();

    auto conn = pool.acquire();
    pool.release(conn);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    pool.cleanup_idle_connections();

    EXPECT_LE(pool.idle_connections(), config_.min_connections);
}
```

### 2.2 Connection Leak Detector Tests

```cpp
// tests/connection_leak_detector_test.cpp
#include <gtest/gtest.h>
#include "database/connection_leak_detector.h"

class LeakDetectorTest : public ::testing::Test {
protected:
    database::connection_leak_detector detector_;
};

// Leak Detection
TEST_F(LeakDetectorTest, DetectUnreleasedConnection) {
    void* fake_conn = reinterpret_cast<void*>(0x12345);

    detector_.track_acquisition(fake_conn, "test_function", 42);

    // Simulate time passage
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto leaks = detector_.check_for_leaks(std::chrono::milliseconds(50));
    EXPECT_EQ(leaks.size(), 1);
    EXPECT_EQ(leaks[0].connection, fake_conn);
    EXPECT_EQ(leaks[0].function_name, "test_function");
}

// Proper Release Tracking
TEST_F(LeakDetectorTest, NoLeakWhenProperlyReleased) {
    void* fake_conn = reinterpret_cast<void*>(0x12345);

    detector_.track_acquisition(fake_conn, "test_function", 42);
    detector_.track_release(fake_conn);

    auto leaks = detector_.check_for_leaks(std::chrono::milliseconds(0));
    EXPECT_TRUE(leaks.empty());
}

// Multiple Connections
TEST_F(LeakDetectorTest, TrackMultipleConnections) {
    for (int i = 1; i <= 5; ++i) {
        void* conn = reinterpret_cast<void*>(i * 0x1000);
        detector_.track_acquisition(conn, "func_" + std::to_string(i), i);
    }

    // Release only some
    detector_.track_release(reinterpret_cast<void*>(0x1000));
    detector_.track_release(reinterpret_cast<void*>(0x3000));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto leaks = detector_.check_for_leaks(std::chrono::milliseconds(50));

    EXPECT_EQ(leaks.size(), 3); // 3 connections not released
}
```

### 2.3 Resilience Integration Tests

```cpp
// tests/resilience_integration_test.cpp
#include <gtest/gtest.h>
#include "database/database_factory.h"

class ResilienceTest : public ::testing::Test {
protected:
    std::unique_ptr<database::database_base> db_;
};

// Auto-Reconnection Test
TEST_F(ResilienceTest, AutoReconnectAfterDisconnect) {
    // Setup: Connect to database
    db_ = database::create_database(database::database_types::SQLite);
    ASSERT_TRUE(db_->connect(":memory:"));

    // Simulate disconnect
    db_->disconnect();

    // Attempt query - should auto-reconnect (depending on implementation)
    auto result = db_->select_query("SELECT 1");
    EXPECT_FALSE(result.empty());
}

// Transaction Rollback on Connection Loss
TEST_F(ResilienceTest, TransactionRollbackOnFailure) {
    db_ = database::create_database(database::database_types::SQLite);
    db_->connect(":memory:");

    db_->execute_query("CREATE TABLE test (id INT)");
    db_->execute_query("BEGIN TRANSACTION");
    db_->insert_query("INSERT INTO test VALUES (1)");

    // Simulate failure before commit
    db_->disconnect();
    db_->connect(":memory:");

    // Verify transaction was rolled back
    auto result = db_->select_query("SELECT COUNT(*) FROM test");
    // Should be 0 or table doesn't exist
}

// Connection Timeout Handling
TEST_F(ResilienceTest, ConnectionTimeoutHandling) {
    db_ = database::create_database(database::database_types::PostgreSQL);

    // Connect to non-existent server with timeout
    bool result = db_->connect(
        "host=192.168.255.255;port=5432;database=test;timeout=1"
    );

    EXPECT_FALSE(result);
    // Should not hang indefinitely
}
```

### 2.4 Implementation Steps

1. **Connection Pool Tests** (Days 1-2)
   - Basic acquire/release tests (4 tests)
   - Pool exhaustion scenario tests (3 tests)
   - Concurrency tests (4 tests)
   - Idle connection cleanup tests (3 tests)

2. **Leak Detector Tests** (Days 2-3)
   - Leak detection tests (3 tests)
   - Tracking accuracy tests (3 tests)
   - Report generation tests (2 tests)

3. **Integration Resilience Tests** (Days 3-4)
   - Auto-reconnection tests (3 tests)
   - Transaction recovery tests (2 tests)
   - Timeout handling tests (2 tests)

4. **Chaos Engineering Scenarios** (Day 5)
   - Network delay simulation
   - Random disconnect tests
   - Resilience verification under load

---

## 3. How to Test

### 3.1 Unit Test Execution

```bash
# Connection Pool tests
ctest -R connection_pool_test -V

# Leak Detector tests
ctest -R leak_detector_test -V

# Resilience integration tests
ctest -R resilience -V
```

### 3.2 Fault Injection Testing

```bash
# Docker-based network failure simulation
docker network disconnect test_network postgres_container

# Latency injection with Toxiproxy
toxiproxy-cli toxic add -n latency -t latency \
  -a latency=1000 postgres_proxy
```

### 3.3 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| Pool test case count | 14+ | ctest count |
| Leak Detector test count | 8+ | ctest count |
| Integration test count | 7+ | ctest count |
| All tests passing | 100% | CI pipeline |
| No memory leaks | 0 leaks | Valgrind/ASAN |

### 3.4 Performance Benchmarks

```cpp
// Pool performance benchmark
TEST_F(ConnectionPoolTest, PerformanceBenchmark) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        auto conn = pool.acquire();
        pool.release(conn);
    }

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    // 10000 acquire/release cycles should complete under 1 second
    EXPECT_LT(ms.count(), 1000);
}
```

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Non-deterministic timing tests | HIGH | Use generous timeout margins |
| Real DB server required | MEDIUM | Prioritize SQLite in-memory |
| Test isolation failures | MEDIUM | Independent pool instances per test |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-004](DB-004-gateway.md) (Gateway - uses connection pooling)
  - [DB-009](DB-009-async-stress.md) (Stress Tests)

---

## 6. Notes

- Valgrind/AddressSanitizer verification required for memory issues
- Run concurrency tests repeatedly to catch race conditions
- Recommend adding test cases based on production failure scenarios

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
