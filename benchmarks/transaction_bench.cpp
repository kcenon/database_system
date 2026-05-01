// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file transaction_bench.cpp
 * @brief Database operation performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <kcenon/database/core/database_backend.h>
#include <kcenon/database/database_types.h>
#include <kcenon/database/query_builder.h>
#include <memory>
#include <vector>

using namespace database;

// Mock database for benchmarking
class mock_transaction_database : public core::database_backend {
public:
    database_types type() const override { return database_types::postgres; }
    kcenon::common::VoidResult initialize(const core::connection_config&) override {
        initialized_ = true;
        return kcenon::common::ok();
    }
    kcenon::common::VoidResult shutdown() override {
        initialized_ = false;
        return kcenon::common::ok();
    }
    bool is_initialized() const override { return initialized_; }
    kcenon::common::Result<core::database_result> select_query(const std::string&) override {
        return core::database_result();
    }
    kcenon::common::VoidResult execute_query(const std::string&) override { return kcenon::common::ok(); }
    kcenon::common::VoidResult begin_transaction() override {
        in_transaction_ = true;
        return kcenon::common::ok();
    }
    kcenon::common::VoidResult commit_transaction() override {
        in_transaction_ = false;
        return kcenon::common::ok();
    }
    kcenon::common::VoidResult rollback_transaction() override {
        in_transaction_ = false;
        return kcenon::common::ok();
    }
    bool in_transaction() const override { return in_transaction_; }
    std::string last_error() const override { return ""; }
    std::map<std::string, std::string> connection_info() const override { return {}; }

private:
    bool initialized_ = false;
    bool in_transaction_ = false;
};

// Benchmark single SELECT query execution
static void BM_Database_SingleSelect(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->initialize({});

    query_builder qb(database_types::postgres);
    auto query = qb.select({"*"})
                   .from("users")
                   .where("id", "=", 123)
                   .build();

    for (auto _ : state) {
        auto result = db->select_query(query);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Database_SingleSelect);

// Benchmark multiple SELECT queries
static void BM_Database_MultipleSelects(benchmark::State& state) {
    int num_queries = state.range(0);
    auto db = std::make_unique<mock_transaction_database>();
    db->initialize({});

    query_builder qb(database_types::postgres);
    auto query = qb.select({"*"})
                   .from("users")
                   .where("id", "=", 123)
                   .build();

    for (auto _ : state) {
        for (int i = 0; i < num_queries; ++i) {
            auto result = db->select_query(query);
            benchmark::DoNotOptimize(result);
        }
    }
}
BENCHMARK(BM_Database_MultipleSelects)->Arg(5)->Arg(10)->Arg(50)->Arg(100);

// Benchmark query execution overhead
static void BM_Database_ExecuteQuery(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->initialize({});

    query_builder qb(database_types::postgres);
    auto query = qb.select({"count(*)"})
                   .from("users")
                   .build();

    for (auto _ : state) {
        auto result = db->execute_query(query);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Database_ExecuteQuery);

// Benchmark batch queries
static void BM_Database_BatchQueries(benchmark::State& state) {
    int batch_size = state.range(0);
    auto db = std::make_unique<mock_transaction_database>();
    db->initialize({});

    for (auto _ : state) {
        for (int i = 0; i < batch_size; ++i) {
            query_builder qb(database_types::postgres);
            auto query = qb.select({"*"})
                           .from("users")
                           .where("id", "=", i)
                           .build();
            auto result = db->select_query(query);
            benchmark::DoNotOptimize(result);
        }
    }
}
BENCHMARK(BM_Database_BatchQueries)->Arg(10)->Arg(100)->Arg(1000);

// Benchmark batch query throughput
static void BM_Database_QueryThroughput(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->initialize({});

    query_builder qb(database_types::postgres);
    auto query = qb.select({"*"})
                   .from("users")
                   .where("active", "=", true)
                   .build();

    const int batch_size = 100;

    for (auto _ : state) {
        for (int i = 0; i < batch_size; ++i) {
            auto result = db->select_query(query);
            benchmark::DoNotOptimize(result);
        }
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Database_QueryThroughput);

// Benchmark mixed query operations
static void BM_Database_MixedOperations(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->initialize({});

    query_builder qb_select(database_types::postgres);
    auto select_query = qb_select.select({"*"})
                                 .from("users")
                                 .where("id", "=", 123)
                                 .build();

    query_builder qb_count(database_types::postgres);
    auto count_query = qb_count.select({"count(*)"})
                               .from("users")
                               .build();

    for (auto _ : state) {
        auto result1 = db->select_query(select_query);
        benchmark::DoNotOptimize(result1);

        auto result2 = db->select_query(count_query);
        benchmark::DoNotOptimize(result2);
    }
}
BENCHMARK(BM_Database_MixedOperations);
