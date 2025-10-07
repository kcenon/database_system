/**
 * @file connection_pool_bench.cpp
 * @brief Connection pool performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include "database/connection_pool.h"
#include "database/database_manager.h"
#include <memory>
#include <thread>
#include <vector>

using namespace database_module;

// Mock database manager for benchmarking (no actual DB connection)
class mock_database : public database_base {
public:
    bool connect() override { return true; }
    bool disconnect() override { return true; }
    bool is_connected() const override { return true; }
    bool execute(const std::string&) override { return true; }
    bool begin_transaction() override { return true; }
    bool commit() override { return true; }
    bool rollback() override { return true; }
    std::string get_last_error() const override { return ""; }
};

// Benchmark connection pool creation
static void BM_ConnectionPool_Create(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 10;

    for (auto _ : state) {
        connection_pool pool(
            config,
            []() { return std::make_unique<mock_database>(); }
        );
        benchmark::DoNotOptimize(pool);
    }
}
BENCHMARK(BM_ConnectionPool_Create);

// Benchmark connection acquisition (single-threaded)
static void BM_ConnectionPool_AcquireSingle(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 10;

    connection_pool pool(
        config,
        []() { return std::make_unique<mock_database>(); }
    );

    for (auto _ : state) {
        auto conn = pool.acquire();
        benchmark::DoNotOptimize(conn);
        // Connection automatically released when going out of scope
    }
}
BENCHMARK(BM_ConnectionPool_AcquireSingle);

// Benchmark connection acquisition and release
static void BM_ConnectionPool_AcquireRelease(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 10;

    connection_pool pool(
        config,
        []() { return std::make_unique<mock_database>(); }
    );

    for (auto _ : state) {
        {
            auto conn = pool.acquire();
            benchmark::DoNotOptimize(conn);
        } // Connection released here
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ConnectionPool_AcquireRelease);

// Benchmark connection acquisition with varying pool sizes
static void BM_ConnectionPool_PoolSize(benchmark::State& state) {
    int pool_size = state.range(0);

    connection_pool_config config;
    config.min_connections = pool_size / 2;
    config.max_connections = pool_size;

    connection_pool pool(
        config,
        []() { return std::make_unique<mock_database>(); }
    );

    for (auto _ : state) {
        auto conn = pool.acquire();
        benchmark::DoNotOptimize(conn);
    }
}
BENCHMARK(BM_ConnectionPool_PoolSize)->Arg(5)->Arg(10)->Arg(20)->Arg(50);

// Benchmark concurrent connection acquisition
static void BM_ConnectionPool_Concurrent(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 10;
    config.max_connections = 50;

    connection_pool pool(
        config,
        []() { return std::make_unique<mock_database>(); }
    );

    for (auto _ : state) {
        auto conn = pool.acquire();
        benchmark::DoNotOptimize(conn);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}
BENCHMARK(BM_ConnectionPool_Concurrent)->Threads(4)->Threads(8)->Threads(16);

// Benchmark pool statistics retrieval
static void BM_ConnectionPool_GetStats(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 10;

    connection_pool pool(
        config,
        []() { return std::make_unique<mock_database>(); }
    );

    for (auto _ : state) {
        auto stats = pool.get_stats();
        benchmark::DoNotOptimize(stats);
    }
}
BENCHMARK(BM_ConnectionPool_GetStats);

// Benchmark connection health check overhead
static void BM_ConnectionPool_HealthCheck(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 5;
    config.max_connections = 10;
    config.enable_health_checks = true;

    connection_pool pool(
        config,
        []() { return std::make_unique<mock_database>(); }
    );

    for (auto _ : state) {
        pool.perform_health_check();
    }
}
BENCHMARK(BM_ConnectionPool_HealthCheck);

// Benchmark connection pool under contention
static void BM_ConnectionPool_Contention(benchmark::State& state) {
    int num_threads = state.range(0);

    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 5; // Intentionally small to create contention

    connection_pool pool(
        config,
        []() { return std::make_unique<mock_database>(); }
    );

    for (auto _ : state) {
        auto conn = pool.acquire();
        benchmark::DoNotOptimize(conn);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}
BENCHMARK(BM_ConnectionPool_Contention)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->UseRealTime();
