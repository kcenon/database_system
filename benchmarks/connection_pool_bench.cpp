// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file connection_pool_bench.cpp
 * @brief Connection pool performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include "database/connection_pool.h"
#include "database/database_base.h"
#include "database/database_types.h"
#include <memory>
#include <thread>
#include <vector>

using namespace database;

// Mock database manager for benchmarking (no actual DB connection)
class mock_database : public database_base {
public:
    database_types database_type() override { return database_types::postgres; }
    bool connect(const std::string&) override { return true; }
    bool disconnect() override { return true; }
    bool create_query(const std::string&) override { return true; }
    unsigned int insert_query(const std::string&) override { return 1; }
    unsigned int update_query(const std::string&) override { return 1; }
    unsigned int delete_query(const std::string&) override { return 1; }
    database_result select_query(const std::string&) override {
        return database_result();
    }
    bool execute_query(const std::string&) override { return true; }
};

// Benchmark connection pool creation
static void BM_ConnectionPool_Create(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 10;

    for (auto _ : state) {
        connection_pool pool(
            database_types::postgres,
            config,
            []() { return std::make_unique<mock_database>(); }
        );
        pool.initialize();
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
        database_types::postgres,
        config,
        []() { return std::make_unique<mock_database>(); }
    );
    pool.initialize();

    for (auto _ : state) {
        auto conn = pool.acquire_connection();
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
        database_types::postgres,
        config,
        []() { return std::make_unique<mock_database>(); }
    );
    pool.initialize();

    for (auto _ : state) {
        {
            auto conn = pool.acquire_connection();
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
        database_types::postgres,
        config,
        []() { return std::make_unique<mock_database>(); }
    );
    pool.initialize();

    for (auto _ : state) {
        auto conn = pool.acquire_connection();
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
        database_types::postgres,
        config,
        []() { return std::make_unique<mock_database>(); }
    );
    pool.initialize();

    for (auto _ : state) {
        auto conn = pool.acquire_connection();
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
        database_types::postgres,
        config,
        []() { return std::make_unique<mock_database>(); }
    );
    pool.initialize();

    for (auto _ : state) {
        auto stats = pool.get_stats();
        benchmark::DoNotOptimize(stats);
    }
}
BENCHMARK(BM_ConnectionPool_GetStats);

// Benchmark connection pool under contention
static void BM_ConnectionPool_Contention(benchmark::State& state) {
    connection_pool_config config;
    config.min_connections = 2;
    config.max_connections = 5; // Intentionally small to create contention

    connection_pool pool(
        database_types::postgres,
        config,
        []() { return std::make_unique<mock_database>(); }
    );
    pool.initialize();

    for (auto _ : state) {
        auto conn = pool.acquire_connection();
        benchmark::DoNotOptimize(conn);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}
BENCHMARK(BM_ConnectionPool_Contention)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->UseRealTime();
