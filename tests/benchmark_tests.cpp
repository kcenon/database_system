/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Database System Project
 */

#include <benchmark/benchmark.h>
#include <memory>

#include "database/database_manager.h"
#include "database/database_types.h"

using namespace database;

// Benchmark database manager operations
static void BM_DatabaseManagerAccess(benchmark::State& state) {
    for (auto _ : state) {
        auto& db = database_manager::handle();
        benchmark::DoNotOptimize(&db);
    }
}
BENCHMARK(BM_DatabaseManagerAccess);

static void BM_DatabaseTypeSettings(benchmark::State& state) {
    auto& db = database_manager::handle();
    for (auto _ : state) {
        db.set_mode(database_types::postgres);
        auto type = db.database_type();
        benchmark::DoNotOptimize(type);
    }
}
BENCHMARK(BM_DatabaseTypeSettings);

static void BM_QueryCreation(benchmark::State& state) {
    auto& db = database_manager::handle();
    db.set_mode(database_types::postgres);

    for (auto _ : state) {
        bool result = db.create_query("SELECT 1");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_QueryCreation);

static void BM_SelectQuery(benchmark::State& state) {
    auto& db = database_manager::handle();
    db.set_mode(database_types::postgres);

    for (auto _ : state) {
        auto result = db.select_query("SELECT 1");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_SelectQuery);

BENCHMARK_MAIN();