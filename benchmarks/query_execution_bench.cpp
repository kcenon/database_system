/**
 * @file query_execution_bench.cpp
 * @brief Query execution performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include "database/query_builder.h"
#include "database/database_manager.h"
#include <memory>

using namespace database_module;

// Benchmark query builder creation
static void BM_QueryBuilder_Create(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
        benchmark::DoNotOptimize(qb);
    }
}
BENCHMARK(BM_QueryBuilder_Create);

// Benchmark SELECT query building
static void BM_QueryBuilder_SelectSimple(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
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
        query_builder qb;
        auto query = qb.select({"id", "name", "email"})
                       .from("users")
                       .where("age > 18")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_SelectWithWhere);

// Benchmark SELECT with multiple conditions
static void BM_QueryBuilder_SelectComplex(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
        auto query = qb.select({"id", "name", "email", "age", "created_at"})
                       .from("users")
                       .where("age > 18")
                       .where("status = 'active'")
                       .order_by("created_at DESC")
                       .limit(100)
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_SelectComplex);

// Benchmark INSERT query building
static void BM_QueryBuilder_Insert(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
        auto query = qb.insert_into("users")
                       .values({{"name", "John"}, {"email", "john@example.com"}})
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_Insert);

// Benchmark UPDATE query building
static void BM_QueryBuilder_Update(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
        auto query = qb.update("users")
                       .set({{"name", "Jane"}, {"email", "jane@example.com"}})
                       .where("id = 123")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_Update);

// Benchmark DELETE query building
static void BM_QueryBuilder_Delete(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
        auto query = qb.delete_from("users")
                       .where("id = 123")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_Delete);

// Benchmark JOIN query building
static void BM_QueryBuilder_Join(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
        auto query = qb.select({"u.id", "u.name", "o.order_id", "o.total"})
                       .from("users u")
                       .join("orders o", "u.id = o.user_id")
                       .where("o.status = 'completed'")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_Join);

// Benchmark parameterized query building
static void BM_QueryBuilder_Parameterized(benchmark::State& state) {
    for (auto _ : state) {
        query_builder qb;
        auto query = qb.select({"*"})
                       .from("users")
                       .where("email = ?")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_Parameterized);

// Benchmark query building with varying complexity
static void BM_QueryBuilder_ComplexityScaling(benchmark::State& state) {
    int num_columns = state.range(0);
    std::vector<std::string> columns;
    for (int i = 0; i < num_columns; ++i) {
        columns.push_back("col" + std::to_string(i));
    }

    for (auto _ : state) {
        query_builder qb;
        auto query = qb.select(columns)
                       .from("test_table")
                       .build();
        benchmark::DoNotOptimize(query);
    }
}
BENCHMARK(BM_QueryBuilder_ComplexityScaling)->Arg(5)->Arg(20)->Arg(50);
