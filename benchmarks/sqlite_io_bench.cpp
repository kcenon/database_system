// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file sqlite_io_bench.cpp
 * @brief Real I/O benchmarks using SQLite in-memory database
 *
 * Measures actual database I/O performance through SQLite :memory: backend.
 * Unlike transaction_bench.cpp which uses mock backends, these benchmarks
 * exercise real SQL parsing, query planning, and B-tree operations.
 *
 * Run: ./build/benchmarks/database_benchmarks --benchmark_filter=SQLite
 */

#ifndef USE_SQLITE
// Provide empty translation unit when SQLite is not compiled in.
// Benchmarks will simply not appear in the output.
#else

#include <benchmark/benchmark.h>
#include <kcenon/database/core/database_context.h>
#include <kcenon/database/database_manager.h>
#include <kcenon/database/database_types.h>
#include <memory>
#include <string>

using namespace database;

namespace {

/**
 * @brief Google Benchmark fixture for SQLite in-memory benchmarks.
 *
 * Creates a database_manager connected to SQLite :memory: with a seeded
 * users table. Follows the same pattern as DatabaseSystemFixture in
 * integration_tests/framework/system_fixture.h.
 */
class SQLiteBenchFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State&) override
    {
        context_ = std::make_shared<database_context>();
        manager_ = std::make_shared<database_manager>(context_);
        manager_->set_mode(database_types::sqlite);

        auto connect_result = manager_->connect_result(":memory:");
        if (connect_result.is_err())
        {
            setup_failed_ = true;
            return;
        }

        // Create test table
        manager_->create_query_result(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL, "
            "email TEXT UNIQUE NOT NULL, "
            "age INTEGER"
            ")");

        // Seed 100 rows for SELECT benchmarks
        for (int i = 0; i < 100; ++i)
        {
            std::string query =
                "INSERT INTO users (name, email, age) VALUES ("
                "'User" + std::to_string(i) + "', "
                "'user" + std::to_string(i) + "@bench.com', " +
                std::to_string(20 + (i % 50)) + ")";
            manager_->execute_query_result(query);
        }
    }

    void TearDown(const ::benchmark::State&) override
    {
        if (manager_)
        {
            manager_->disconnect_result();
        }
    }

protected:
    std::shared_ptr<database_context> context_;
    std::shared_ptr<database_manager> manager_;
    bool setup_failed_{false};
};

// ============================================================================
// BM_SQLite_SingleSelect
// Measures single SELECT with WHERE clause against real SQLite engine.
// ============================================================================

BENCHMARK_DEFINE_F(SQLiteBenchFixture, SQLite_SingleSelect)(benchmark::State& state)
{
    if (setup_failed_)
    {
        state.SkipWithError("SQLite setup failed");
        return;
    }

    for (auto _ : state)
    {
        auto result = manager_->select_query_result(
            "SELECT id, name, email, age FROM users WHERE id = 42");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(SQLiteBenchFixture, SQLite_SingleSelect);

// ============================================================================
// BM_SQLite_BatchInsert
// Measures INSERT throughput at different batch sizes.
// Each iteration re-inserts into a separate table to avoid unique violations.
// ============================================================================

BENCHMARK_DEFINE_F(SQLiteBenchFixture, SQLite_BatchInsert)(benchmark::State& state)
{
    if (setup_failed_)
    {
        state.SkipWithError("SQLite setup failed");
        return;
    }

    const auto batch_size = state.range(0);

    // Create a scratch table for inserts
    manager_->create_query_result(
        "CREATE TABLE IF NOT EXISTS bench_insert ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "name TEXT NOT NULL, "
        "value INTEGER"
        ")");

    for (auto _ : state)
    {
        for (int64_t i = 0; i < batch_size; ++i)
        {
            auto result = manager_->execute_query_result(
                "INSERT INTO bench_insert (name, value) VALUES ("
                "'item" + std::to_string(i) + "', " + std::to_string(i) + ")");
            benchmark::DoNotOptimize(result);
        }
    }

    state.SetItemsProcessed(state.iterations() * batch_size);

    // Cleanup
    manager_->execute_query_result("DELETE FROM bench_insert");
}
BENCHMARK_REGISTER_F(SQLiteBenchFixture, SQLite_BatchInsert)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000);

// ============================================================================
// BM_SQLite_TransactionCommit
// Measures BEGIN + INSERT + COMMIT as a single unit.
// ============================================================================

BENCHMARK_DEFINE_F(SQLiteBenchFixture, SQLite_TransactionCommit)(benchmark::State& state)
{
    if (setup_failed_)
    {
        state.SkipWithError("SQLite setup failed");
        return;
    }

    manager_->create_query_result(
        "CREATE TABLE IF NOT EXISTS bench_txn ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "data TEXT"
        ")");

    for (auto _ : state)
    {
        manager_->begin_transaction();
        auto result = manager_->execute_query_result(
            "INSERT INTO bench_txn (data) VALUES ('txn_test')");
        benchmark::DoNotOptimize(result);
        manager_->commit_transaction();
    }

    // Cleanup
    manager_->execute_query_result("DELETE FROM bench_txn");
}
BENCHMARK_REGISTER_F(SQLiteBenchFixture, SQLite_TransactionCommit);

// ============================================================================
// BM_SQLite_SelectThroughput
// Measures SELECT throughput in batches of 100 queries, reporting items/sec.
// ============================================================================

BENCHMARK_DEFINE_F(SQLiteBenchFixture, SQLite_SelectThroughput)(benchmark::State& state)
{
    if (setup_failed_)
    {
        state.SkipWithError("SQLite setup failed");
        return;
    }

    constexpr int batch_size = 100;

    for (auto _ : state)
    {
        for (int i = 0; i < batch_size; ++i)
        {
            auto result = manager_->select_query_result(
                "SELECT id, name, email FROM users WHERE age = " +
                std::to_string(20 + (i % 50)));
            benchmark::DoNotOptimize(result);
        }
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK_REGISTER_F(SQLiteBenchFixture, SQLite_SelectThroughput);

}  // namespace

#endif  // USE_SQLITE
