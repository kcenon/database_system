# Testing Plan

## 🎯 Testing Strategy

This document outlines the comprehensive testing strategy for the database system integration.

## 📊 Test Coverage Goals

- **Unit Tests**: 80%+ code coverage
- **Integration Tests**: All integration points tested
- **Performance Tests**: Baseline established, <5% overhead acceptable
- **Memory Tests**: Zero leaks, zero data races

---

## 🧪 Test Levels

### 1. Unit Tests

**Location**: `tests/integrated/`

**Scope**: Individual components in isolation

#### 1.1 Configuration Tests

**File**: `test_configuration.cpp`

```cpp
TEST(ConfigurationTest, DefaultValues) {
    unified_db_config config;
    EXPECT_EQ(config.connection_pool.min_connections, 5);
    EXPECT_EQ(config.connection_pool.max_connections, 50);
    EXPECT_EQ(config.logger.min_log_level, db_log_level::info);
}

TEST(ConfigurationTest, BuilderPattern) {
    auto config = unified_db_config{}
        .set_backend(backend_type::postgres, "host=localhost")
        .set_pool_size(10, 100)
        .set_log_level(db_log_level::debug);

    EXPECT_EQ(config.database.type, backend_type::postgres);
    EXPECT_EQ(config.connection_pool.min_connections, 10);
    EXPECT_EQ(config.logger.min_log_level, db_log_level::debug);
}
```

#### 1.2 Logger Adapter Tests

**File**: `test_logger_adapter.cpp`

```cpp
TEST(LoggerAdapterTest, InitializeWithLoggerSystem) {
    #if defined(USE_LOGGER_SYSTEM)
    db_logger_config config;
    config.enable_file_logging = false;  // Test only console

    logger_adapter adapter(config);
    auto result = adapter.initialize();

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(adapter.is_initialized());
    #else
    GTEST_SKIP() << "logger_system not available";
    #endif
}

TEST(LoggerAdapterTest, FallbackLogging) {
    #if !defined(USE_LOGGER_SYSTEM)
    db_logger_config config;
    logger_adapter adapter(config);

    auto result = adapter.initialize();
    EXPECT_TRUE(result.is_ok());

    // Should not crash even without logger_system
    adapter.log(db_log_level::info, "Test message");
    adapter.flush();
    #else
    GTEST_SKIP() << "Testing fallback, but logger_system is available";
    #endif
}

TEST(LoggerAdapterTest, SQLSanitization) {
    db_logger_config config;
    logger_adapter adapter(config);
    adapter.initialize();

    // Should remove password
    std::string query = "CREATE USER foo WITH PASSWORD 'secret123'";
    adapter.log_query(db_log_level::debug, query, std::chrono::microseconds{100});

    // Check log output doesn't contain 'secret123'
    // (Implementation detail: capture stdout or check log file)
}

TEST(LoggerAdapterTest, SlowQueryDetection) {
    db_logger_config config;
    config.slow_query_threshold = std::chrono::milliseconds{100};
    logger_adapter adapter(config);
    adapter.initialize();

    // Slow query should trigger warning
    auto slow_duration = std::chrono::microseconds{150000};  // 150ms
    adapter.log_query(db_log_level::info, "SELECT * FROM large_table", slow_duration);

    // Verify warning logged (check output)
}
```

#### 1.3 Monitoring Adapter Tests

**File**: `test_monitoring_adapter.cpp`

```cpp
TEST(MonitoringAdapterTest, RecordMetrics) {
    db_monitoring_config config;
    monitoring_adapter adapter(config);
    adapter.initialize();

    adapter.record_query_execution(std::chrono::microseconds{500}, true);
    adapter.record_query_execution(std::chrono::microseconds{1000}, true);
    adapter.record_query_execution(std::chrono::microseconds{200}, false);

    auto metrics_result = adapter.get_database_metrics();
    EXPECT_TRUE(metrics_result.is_ok());

    auto metrics = metrics_result.value();
    EXPECT_EQ(metrics.total_queries, 3);
    EXPECT_EQ(metrics.successful_queries, 2);
    EXPECT_EQ(metrics.failed_queries, 1);
    EXPECT_DOUBLE_EQ(metrics.query_success_rate, 2.0/3.0);
}

TEST(MonitoringAdapterTest, HealthCheck) {
    db_monitoring_config config;
    monitoring_adapter adapter(config);
    adapter.initialize();

    // Simulate healthy pool
    adapter.update_pool_stats(10, 40, 50);  // 20% usage

    auto health_result = adapter.check_health();
    EXPECT_TRUE(health_result.is_ok());
    EXPECT_TRUE(health_result.value().is_healthy);
}

TEST(MonitoringAdapterTest, PrometheusExport) {
    #if defined(USE_MONITORING_SYSTEM)
    db_monitoring_config config;
    config.enable_prometheus_export = true;
    monitoring_adapter adapter(config);
    adapter.initialize();

    adapter.record_query_execution(std::chrono::microseconds{500}, true);

    auto metrics = adapter.get_metrics();
    // Verify Prometheus format
    // (Check for metric names like database_queries_total, database_query_latency_seconds)
    #else
    GTEST_SKIP();
    #endif
}
```

#### 1.4 Thread Adapter Tests

**File**: `test_thread_adapter.cpp`

```cpp
TEST(ThreadAdapterTest, SubmitTask) {
    db_thread_config config;
    config.thread_count = 4;
    thread_adapter adapter(config);
    adapter.initialize();

    std::atomic<int> counter{0};
    auto future = adapter.submit([&counter]() { counter++; });
    future.get();

    EXPECT_EQ(counter.load(), 1);
}

TEST(ThreadAdapterTest, PriorityScheduling) {
    db_thread_config config;
    config.enable_priority_scheduling = true;
    thread_adapter adapter(config);
    adapter.initialize();

    std::vector<int> execution_order;
    std::mutex mutex;

    // Submit low priority
    auto low = adapter.submit_with_priority(1, [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(1);
    });

    // Submit high priority
    auto high = adapter.submit_with_priority(100, [&]() {
        std::lock_guard<std::mutex> lock(mutex);
        execution_order.push_back(100);
    });

    high.get();
    low.get();

    // High priority should execute first (if queue was not empty)
    // Note: This is probabilistic, may need multiple iterations
}

TEST(ThreadAdapterTest, CancellationToken) {
    db_thread_config config;
    thread_adapter adapter(config);
    adapter.initialize();

    auto token = adapter.create_cancellation_token();

    std::atomic<bool> task_executed{false};
    auto future = adapter.submit_cancellable(token, [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        task_executed = true;
    });

    // Cancel immediately
    adapter.cancel_token(token);

    // Task should throw or return early
    EXPECT_THROW(future.get(), std::runtime_error);
    EXPECT_FALSE(task_executed.load());
}
```

#### 1.5 Database Coordinator Tests

**File**: `test_database_coordinator.cpp`

```cpp
TEST(DatabaseCoordinatorTest, InitializationOrder) {
    unified_db_config config;
    config.set_backend(backend_type::postgres, "host=localhost");

    database_coordinator coordinator(config);
    auto result = coordinator.initialize();

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(coordinator.is_initialized());

    // Verify all adapters initialized
    EXPECT_NE(coordinator.get_logger(), nullptr);
    EXPECT_NE(coordinator.get_monitor(), nullptr);
    EXPECT_NE(coordinator.get_thread_pool(), nullptr);
}

TEST(DatabaseCoordinatorTest, ShutdownOrder) {
    unified_db_config config;
    database_coordinator coordinator(config);
    coordinator.initialize();

    auto result = coordinator.shutdown();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(coordinator.is_initialized());
}

TEST(DatabaseCoordinatorTest, ErrorHandling) {
    unified_db_config config;
    config.logger.log_directory = "/invalid/path/that/does/not/exist";

    database_coordinator coordinator(config);
    auto result = coordinator.initialize();

    // Should handle error gracefully
    if (!result.is_ok()) {
        EXPECT_FALSE(result.error().message.empty());
    }
}
```

---

### 2. Integration Tests

**Location**: `integration_tests/`

**Scope**: Multiple components working together

#### 2.1 Connection Pool + Logger Integration

**File**: `test_pool_logger_integration.cpp`

```cpp
TEST(IntegrationTest, ConnectionPoolLogsEvents) {
    // Create logger
    db_logger_config logger_config;
    logger_adapter logger(logger_config);
    logger.initialize();

    // Create connection pool with logger
    connection_pool_v2::config pool_config;
    pool_config.min_size = 5;
    pool_config.max_size = 10;

    connection_pool_v2 pool(pool_config, &logger);
    pool.initialize();

    // Acquire connection - should log
    auto conn_result = pool.acquire_connection(connection_priority::NORMAL_QUERY);
    EXPECT_TRUE(conn_result.is_ok());

    // Release connection - should log
    pool.release_connection(conn_result.value());

    // Check logs (capture stdout or read log file)
    logger.flush();
}
```

#### 2.2 Connection Pool + Monitoring Integration

**File**: `test_pool_monitor_integration.cpp`

```cpp
TEST(IntegrationTest, ConnectionPoolReportsMetrics) {
    // Create monitoring
    db_monitoring_config monitor_config;
    monitoring_adapter monitor(monitor_config);
    monitor.initialize();

    // Create connection pool with monitor
    connection_pool_v2::config pool_config;
    connection_pool_v2 pool(pool_config, nullptr, &monitor);
    pool.initialize();

    // Acquire/release connections
    auto conn1 = pool.acquire_connection(connection_priority::NORMAL_QUERY);
    auto conn2 = pool.acquire_connection(connection_priority::NORMAL_QUERY);

    // Check metrics
    auto metrics = monitor.get_database_metrics();
    EXPECT_TRUE(metrics.is_ok());
    EXPECT_EQ(metrics.value().active_connections, 2);

    pool.release_connection(conn1.value());
    pool.release_connection(conn2.value());
}
```

#### 2.3 Full Stack Integration

**File**: `test_unified_database_system.cpp`

```cpp
TEST(IntegrationTest, UnifiedSystemEndToEnd) {
    unified_db_config config;
    config.set_backend(backend_type::postgres, "host=localhost dbname=test")
          .set_pool_size(5, 20)
          .set_log_level(db_log_level::debug)
          .enable_monitoring(true);

    unified_database_system db(config);
    auto init_result = db.initialize();
    EXPECT_TRUE(init_result.is_ok());

    // Execute query
    auto query_result = db.execute_query("SELECT 1 as num");
    EXPECT_TRUE(query_result.is_ok());

    // Check metrics
    auto metrics = db.get_metrics();
    EXPECT_TRUE(metrics.is_ok());
    EXPECT_GT(metrics.value().total_queries, 0);

    // Shutdown
    auto shutdown_result = db.shutdown();
    EXPECT_TRUE(shutdown_result.is_ok());
}
```

---

### 3. Performance Tests

**Location**: `benchmarks/`

#### 3.1 Adapter Overhead

**File**: `benchmark_adapter_overhead.cpp`

```cpp
void BM_DirectLogging(benchmark::State& state) {
    for (auto _ : state) {
        std::cout << "Log message\n";
    }
}
BENCHMARK(BM_DirectLogging);

void BM_LoggerAdapter(benchmark::State& state) {
    db_logger_config config;
    logger_adapter logger(config);
    logger.initialize();

    for (auto _ : state) {
        logger.log(db_log_level::info, "Log message");
    }
}
BENCHMARK(BM_LoggerAdapter);

// Target: logger_adapter overhead < 100ns per call
```

#### 3.2 Connection Pool Performance

**File**: `benchmark_connection_pool.cpp`

```cpp
void BM_ConnectionAcquisition(benchmark::State& state) {
    unified_db_config config;
    unified_database_system db(config);
    db.initialize();

    for (auto _ : state) {
        auto result = db.execute_query("SELECT 1");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ConnectionAcquisition);

// Target: Maintain existing throughput (within 5%)
```

---

### 4. Memory Safety Tests

#### 4.1 AddressSanitizer (ASan)

```bash
# Build with ASan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
      -DCMAKE_BUILD_TYPE=Debug \
      -S . -B build-asan

cmake --build build-asan
./build-asan/tests/all_tests
```

**Expected**: No memory leaks, no use-after-free, no buffer overflows

#### 4.2 ThreadSanitizer (TSan)

```bash
# Build with TSan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
      -DCMAKE_BUILD_TYPE=Debug \
      -S . -B build-tsan

cmake --build build-tsan
./build-tsan/tests/all_tests
```

**Expected**: No data races, no deadlocks

#### 4.3 LeakSanitizer (LSan)

```bash
# Build with LSan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=leak -fno-omit-frame-pointer" \
      -DCMAKE_BUILD_TYPE=Debug \
      -S . -B build-lsan

cmake --build build-lsan
./build-lsan/tests/all_tests
```

**Expected**: Zero leaks at shutdown

---

### 5. Compatibility Tests

#### 5.1 Build Matrix

Test all combinations:

| System | Enabled | Disabled |
|--------|---------|----------|
| common_system | ✅ | ✅ |
| thread_system | ✅ | ✅ |
| logger_system | ✅ | ✅ |
| monitoring_system | ✅ | ✅ |

**Total combinations**: 2^4 = 16

**Priority combinations** (test these first):
1. All enabled
2. All disabled
3. Only logger enabled
4. Only monitoring enabled
5. Logger + monitoring enabled

#### 5.2 Compiler Matrix

Test on:
- [ ] GCC 10, 11, 12, 13
- [ ] Clang 12, 13, 14, 15, 16
- [ ] MSVC 2019, 2022 (if Windows support)

#### 5.3 Platform Matrix

- [ ] Linux (Ubuntu 20.04, 22.04)
- [ ] macOS (12, 13, 14)
- [ ] Windows (if supported)

---

## 📋 Test Execution Plan

### Phase-by-Phase Testing

Each phase has specific test requirements (see phase checklists).

### Continuous Integration

**On every commit**:
- [ ] Unit tests
- [ ] Static analysis (clang-tidy, cppcheck)
- [ ] Compiler warnings check

**On pull request**:
- [ ] Integration tests
- [ ] Performance tests (compare to baseline)
- [ ] Memory tests (ASan, TSan, LSan)
- [ ] Build matrix (subset)

**Before merge**:
- [ ] Full build matrix
- [ ] Manual review
- [ ] Documentation review

---

## ✅ Success Criteria

- [ ] All unit tests pass (100%)
- [ ] Integration tests pass (100%)
- [ ] Performance overhead < 5%
- [ ] Zero memory leaks
- [ ] Zero data races
- [ ] Build on all supported compilers
- [ ] Work with all system combinations

---

**Document Version**: 1.0
**Last Updated**: 2025-11-03
