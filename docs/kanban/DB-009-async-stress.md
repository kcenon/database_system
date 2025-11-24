# DB-009: Async Operation Stress Tests

**Category**: TEST
**Priority**: MEDIUM
**Status**: TODO
**Est. Duration**: 3-4 days
**Dependencies**: None
**Assignee**: TBD
**Created**: 2025-11-24

---

## 1. What to Change (무엇을 바꾸려는 것인지?)

### Current State
- 비동기 데이터베이스 작업이 지원되나 스트레스 테스트 부재
- 고부하 상황에서의 동작 검증 없음
- 메모리 누수, 데드락 등의 동시성 이슈 미검증
- 대량 동시 연결 시나리오 테스트 없음

### Target State
- 고부하 환경에서의 안정성 검증
- 메모리 사용량 한계 테스트
- 동시 연결 한계 및 복구 테스트
- 장시간 운영 시뮬레이션 (Soak Test)

### Scope
**대상 영역**:
- Async Query Execution
- Connection Pool under Load
- Memory Management
- Thread Safety

**추가할 테스트 파일**:
- `tests/stress/async_stress_test.cpp`
- `tests/stress/connection_stress_test.cpp`
- `tests/stress/memory_stress_test.cpp`

---

## 2. How to Change (어떻게 바꾸려고 하는 것인지?)

### 2.1 Async Operation Stress Tests

```cpp
// tests/stress/async_stress_test.cpp
#include <gtest/gtest.h>
#include "database/database_base.h"
#include <future>
#include <vector>
#include <atomic>

class AsyncStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = database::create_database(database::database_types::SQLite);
        db_->connect(":memory:");
        db_->execute_query(
            "CREATE TABLE stress_test (id INTEGER PRIMARY KEY, value TEXT)"
        );
    }

    std::unique_ptr<database::database_base> db_;
};

// High Concurrency Test
TEST_F(AsyncStressTest, HighConcurrencyInserts) {
    constexpr int NUM_THREADS = 100;
    constexpr int OPS_PER_THREAD = 100;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::future<void>> futures;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        futures.push_back(std::async(std::launch::async, [&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string query = "INSERT INTO stress_test (value) VALUES ('" +
                                    std::to_string(t * OPS_PER_THREAD + i) + "')";
                if (db_->insert_query(query) > 0) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }

    auto duration = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    // Report results
    std::cout << "Completed " << success_count << " successful ops, "
              << failure_count << " failures in " << ms.count() << "ms\n";

    // At least 90% should succeed
    EXPECT_GE(success_count.load(), NUM_THREADS * OPS_PER_THREAD * 0.9);
}

// Mixed Read/Write Workload
TEST_F(AsyncStressTest, MixedReadWriteWorkload) {
    constexpr int NUM_WRITERS = 10;
    constexpr int NUM_READERS = 50;
    constexpr int DURATION_MS = 5000;

    std::atomic<bool> running{true};
    std::atomic<int> write_ops{0};
    std::atomic<int> read_ops{0};

    std::vector<std::thread> threads;

    // Writers
    for (int i = 0; i < NUM_WRITERS; ++i) {
        threads.emplace_back([&, i]() {
            int counter = 0;
            while (running) {
                std::string query = "INSERT INTO stress_test (value) VALUES ('" +
                                    std::to_string(i * 100000 + counter++) + "')";
                db_->insert_query(query);
                write_ops++;
            }
        });
    }

    // Readers
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([&]() {
            while (running) {
                db_->select_query("SELECT * FROM stress_test LIMIT 100");
                read_ops++;
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Write ops: " << write_ops << ", Read ops: " << read_ops << "\n";

    EXPECT_GT(write_ops.load(), 0);
    EXPECT_GT(read_ops.load(), 0);
}

// Query Timeout Under Load
TEST_F(AsyncStressTest, QueryTimeoutUnderLoad) {
    // Generate load
    std::vector<std::thread> load_threads;
    std::atomic<bool> generating_load{true};

    for (int i = 0; i < 50; ++i) {
        load_threads.emplace_back([&]() {
            while (generating_load) {
                db_->select_query("SELECT * FROM stress_test");
            }
        });
    }

    // Test timeout functionality
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Attempt a slow query - should timeout gracefully
    // (Implementation dependent)

    generating_load = false;
    for (auto& t : load_threads) {
        t.join();
    }

    // System should remain responsive
    auto result = db_->select_query("SELECT 1");
    EXPECT_FALSE(result.empty());
}
```

### 2.2 Connection Stress Tests

```cpp
// tests/stress/connection_stress_test.cpp
#include <gtest/gtest.h>
#include "database/connection_pool.h"

class ConnectionStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_connections = 50;
        config_.connection_string = ":memory:";
    }

    pool_config config_;
};

// Rapid Connection Cycling
TEST_F(ConnectionStressTest, RapidConnectionCycling) {
    constexpr int CYCLES = 1000;

    for (int i = 0; i < CYCLES; ++i) {
        auto db = database::create_database(database::database_types::SQLite);
        ASSERT_TRUE(db->connect(":memory:"));
        db->execute_query("SELECT 1");
        db->disconnect();
    }

    // No resource leaks (check via external tools)
    SUCCEED();
}

// Pool Exhaustion and Recovery
TEST_F(ConnectionStressTest, PoolExhaustionRecovery) {
    ConnectionPool pool(config_);
    pool.initialize();

    // Acquire all connections
    std::vector<Connection*> connections;
    for (int i = 0; i < config_.max_connections; ++i) {
        auto conn = pool.acquire();
        ASSERT_NE(conn, nullptr);
        connections.push_back(conn);
    }

    // Pool should be exhausted
    auto extra = pool.try_acquire(std::chrono::milliseconds(100));
    EXPECT_EQ(extra, nullptr);

    // Release half
    for (int i = 0; i < config_.max_connections / 2; ++i) {
        pool.release(connections[i]);
    }

    // Should be able to acquire again
    auto recovered = pool.acquire();
    EXPECT_NE(recovered, nullptr);

    // Cleanup
    pool.release(recovered);
    for (int i = config_.max_connections / 2; i < config_.max_connections; ++i) {
        pool.release(connections[i]);
    }
}

// Connection Leak Under Stress
TEST_F(ConnectionStressTest, NoLeaksUnderStress) {
    ConnectionPool pool(config_);
    pool.initialize();

    std::atomic<int> leak_count{0};

    auto stress_fn = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto conn = pool.acquire();
            if (conn) {
                // Simulate work
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                pool.release(conn);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 20; ++i) {
        threads.emplace_back(stress_fn);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All connections should be returned
    EXPECT_EQ(pool.active_connections(), 0);
}
```

### 2.3 Memory Stress Tests

```cpp
// tests/stress/memory_stress_test.cpp
#include <gtest/gtest.h>
#include "database/database_base.h"

class MemoryStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = database::create_database(database::database_types::SQLite);
        db_->connect(":memory:");
    }

    std::unique_ptr<database::database_base> db_;
};

// Large Result Set Handling
TEST_F(MemoryStressTest, LargeResultSetMemory) {
    // Create large table
    db_->execute_query(
        "CREATE TABLE large_data (id INTEGER PRIMARY KEY, data TEXT)"
    );

    // Insert 10000 rows
    for (int i = 0; i < 10000; ++i) {
        std::string data(1000, 'X'); // 1KB per row
        db_->insert_query(
            "INSERT INTO large_data (data) VALUES ('" + data + "')"
        );
    }

    // Query all data
    size_t initial_memory = get_current_memory_usage();

    auto result = db_->select_query("SELECT * FROM large_data");

    size_t after_query_memory = get_current_memory_usage();

    // Memory should grow reasonably (~10MB for 10000 * 1KB)
    EXPECT_LT(after_query_memory - initial_memory, 50 * 1024 * 1024);

    // After clearing result, memory should be released
    result.clear();

    // Note: May need GC or explicit deallocation depending on implementation
}

// Repeated Query Memory Stability
TEST_F(MemoryStressTest, RepeatedQueryMemoryStability) {
    db_->execute_query("CREATE TABLE test (id INT, value TEXT)");
    for (int i = 0; i < 100; ++i) {
        db_->insert_query("INSERT INTO test VALUES (" + std::to_string(i) + ", 'test')");
    }

    size_t baseline_memory = get_current_memory_usage();

    // Execute many queries
    for (int i = 0; i < 10000; ++i) {
        auto result = db_->select_query("SELECT * FROM test");
        // Result goes out of scope, should be freed
    }

    size_t final_memory = get_current_memory_usage();

    // Memory should not grow significantly (< 10% increase)
    EXPECT_LT(final_memory, baseline_memory * 1.1);
}

private:
    size_t get_current_memory_usage() {
        // Platform-specific memory measurement
        #ifdef __APPLE__
        struct mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count);
        return info.resident_size;
        #else
        return 0; // Implement for other platforms
        #endif
    }
};
```

### 2.4 Implementation Steps

1. **Async 스트레스 테스트** (Day 1-2)
   - 고동시성 삽입 테스트 (1개)
   - 혼합 읽기/쓰기 워크로드 (1개)
   - 타임아웃 동작 검증 (1개)
   - 예외 처리 검증 (2개)

2. **Connection 스트레스 테스트** (Day 2-3)
   - 빠른 연결 순환 테스트 (1개)
   - 풀 고갈/복구 테스트 (1개)
   - 누수 검증 테스트 (1개)

3. **Memory 스트레스 테스트** (Day 3-4)
   - 대용량 결과 처리 (1개)
   - 반복 쿼리 메모리 안정성 (1개)
   - 장시간 실행 테스트 (1개)

---

## 3. How to Test (어떻게 테스트 할 것인지?)

### 3.1 Test Execution

```bash
# 전체 스트레스 테스트 실행
ctest -R stress -V --timeout 300

# 개별 테스트
ctest -R async_stress -V
ctest -R connection_stress -V
ctest -R memory_stress -V
```

### 3.2 Memory Leak Detection

```bash
# Valgrind (Linux)
valgrind --leak-check=full --show-leak-kinds=all \
  ./build/tests/stress_tests

# AddressSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
cmake --build build
./build/tests/stress_tests
```

### 3.3 Performance Monitoring

```bash
# CPU/Memory profiling during stress test
perf record ./build/tests/async_stress_test
perf report

# Memory tracking
heaptrack ./build/tests/memory_stress_test
heaptrack_print heaptrack.*.gz
```

### 3.4 Acceptance Criteria

| Criteria | Target | Verification |
|----------|--------|--------------|
| 동시 연결 지원 | 100+ | async_stress_test |
| 메모리 누수 | 0 bytes | Valgrind/ASAN |
| 테스트 완료 시간 | < 5분 | ctest timeout |
| 성공률 | > 95% | 테스트 결과 |
| 데드락 | 0건 | ThreadSanitizer |

### 3.5 Soak Test (Long-Running)

```bash
# 1시간 연속 운영 테스트
./build/tests/soak_test --duration=3600

# 기대 결과:
# - 메모리 사용량 안정적 유지
# - 처리량 일정 수준 유지
# - 에러율 < 0.1%
```

---

## 4. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| 테스트 환경 리소스 부족 | HIGH | CI 전용 고사양 러너 사용 |
| 비결정적 실패 | MEDIUM | 여러 번 반복 실행으로 검증 |
| 테스트 시간 과다 | LOW | 병렬 실행, 타임아웃 설정 |

---

## 5. Related Tickets

- **Blocks**: None
- **Blocked by**: None
- **Related**:
  - [DB-003](DB-003-resilience-tests.md) (Resilience Tests)
  - [DB-007](DB-007-benchmark.md) (Performance Benchmark)

---

## 6. Notes

- CI 환경에서는 축소된 스트레스 레벨로 실행
- 전체 스트레스 테스트는 주간 스케줄로 실행 권장
- ThreadSanitizer로 레이스 컨디션 검증 필수

---

**Document Author**: Claude
**Last Modified**: 2025-11-24
