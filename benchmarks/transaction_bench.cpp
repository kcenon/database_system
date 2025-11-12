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
 * @file transaction_bench.cpp
 * @brief Database operation performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include "database/database_base.h"
#include "database/database_types.h"
#include "database/query_builder.h"
#include <memory>
#include <vector>

using namespace database;

// Mock database for benchmarking
class mock_transaction_database : public database_base {
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

// Benchmark single SELECT query execution
static void BM_Database_SingleSelect(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->connect("");

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
    db->connect("");

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
    db->connect("");

    query_builder qb(database_types::postgres);
    auto query = qb.select({"count(*)"})
                   .from("users")
                   .build();

    for (auto _ : state) {
        bool success = db->execute_query(query);
        benchmark::DoNotOptimize(success);
    }
}
BENCHMARK(BM_Database_ExecuteQuery);

// Benchmark batch queries
static void BM_Database_BatchQueries(benchmark::State& state) {
    int batch_size = state.range(0);
    auto db = std::make_unique<mock_transaction_database>();
    db->connect("");

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
    db->connect("");

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
    db->connect("");

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
