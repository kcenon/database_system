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
 * @file query_execution_bench.cpp
 * @brief Query execution performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include "database/query_builder.h"
#include "database/database_types.h"
#include <memory>

using namespace database;

// Benchmark query builder creation
static void BM_QueryBuilder_Create(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb(database_types::postgres);
        benchmark::DoNotOptimize(qb);
    }
}
BENCHMARK(BM_QueryBuilder_Create);

// Benchmark SELECT query building
static void BM_QueryBuilder_SelectSimple(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb(database_types::postgres);
        auto query = qb.select({"id", "name", "email"})
                       .from("users")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_SelectSimple);

// Benchmark SELECT with WHERE clause
static void BM_QueryBuilder_SelectWithWhere(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb(database_types::postgres);
        auto query = qb.select({"id", "name", "email"})
                       .from("users")
                       .where("age", ">", 18)
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_SelectWithWhere);

// Benchmark SELECT with multiple conditions
static void BM_QueryBuilder_SelectComplex(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb(database_types::postgres);
        auto query = qb.select({"id", "name", "email", "age", "created_at"})
                       .from("users")
                       .where("age", ">", 18)
                       .where("status", "=", "active")
                       .order_by("created_at", sort_order::desc)
                       .limit(100)
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_SelectComplex);

// Benchmark JOIN query building
static void BM_QueryBuilder_Join(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb(database_types::postgres);
        auto query = qb.select({"u.id", "u.name", "o.order_id", "o.total"})
                       .from("users u")
                       .join("orders o", "u.id = o.user_id")
                       .where("o.status", "=", "completed")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_Join);

// Benchmark query building with varying complexity
static void BM_QueryBuilder_ComplexityScaling(benchmark::State& state) {
    int num_columns = state.range(0);
    std::vector<std::string> columns;
    for (int i = 0; i < num_columns; ++i) {
        columns.push_back("col" + std::to_string(i));
    }

    for (auto _ : state) {
        query_builder qb(database_types::postgres);
        auto query = qb.select(columns)
                       .from("test_table")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_ComplexityScaling)->Arg(5)->Arg(20)->Arg(50);
