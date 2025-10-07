/**
 * @file transaction_bench.cpp
 * @brief Transaction performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include "database/database_base.h"
#include "database/database_types.h"
#include "database/query_builder.h"
#include <memory>
#include <vector>

using namespace database;

// Mock database for transaction benchmarking
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

// Benchmark single query execution
static void BM_Transaction_SingleQuery(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->connect("");

    query_builder qb(database_types::postgres);
    std::map<std::string, database_value> data = {
        {"name", "John"},
        {"email", "john@example.com"}
    };
    auto query = qb.insert(data).build();

    for (auto _ : state) {
        db->insert_query(query);
    }
}
BENCHMARK(BM_Transaction_SingleQuery);

// Benchmark multiple queries
static void BM_Transaction_MultipleQueries(benchmark::State& state) {
    int num_queries = state.range(0);
    auto db = std::make_unique<mock_transaction_database>();
    db->connect("");

    query_builder qb(database_types::postgres);
    std::map<std::string, database_value> data = {
        {"name", "John"},
        {"email", "john@example.com"}
    };
    auto query = qb.insert(data).build();

    for (auto _ : state) {
        for (int i = 0; i < num_queries; ++i) {
            db->execute_query(query);
        }
    }
}
BENCHMARK(BM_Transaction_MultipleQueries)->Arg(5)->Arg(10)->Arg(50)->Arg(100);

// Benchmark batch insert
static void BM_Transaction_BatchInsert(benchmark::State& state) {
    int batch_size = state.range(0);
    auto db = std::make_unique<mock_transaction_database>();
    db->connect("");

    query_builder qb(database_types::postgres);

    for (auto _ : state) {
        for (int i = 0; i < batch_size; ++i) {
            std::map<std::string, database_value> data = {
                {"name", "User" + std::to_string(i)},
                {"email", "user" + std::to_string(i) + "@example.com"},
                {"age", 20 + (i % 50)}
            };
            auto query = qb.insert(data).build();
            db->insert_query(query);
        }
    }
}
BENCHMARK(BM_Transaction_BatchInsert)->Arg(10)->Arg(100)->Arg(1000);

// Benchmark batch insert throughput
static void BM_Transaction_BatchInsertThroughput(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->connect("");

    query_builder qb(database_types::postgres);
    const int batch_size = 100;

    for (auto _ : state) {
        for (int i = 0; i < batch_size; ++i) {
            std::map<std::string, database_value> data = {
                {"name", "User" + std::to_string(i)},
                {"email", "user" + std::to_string(i) + "@example.com"},
                {"age", 20 + (i % 50)}
            };
            auto query = qb.insert(data).build();
            db->insert_query(query);
        }
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_Transaction_BatchInsertThroughput);

// Benchmark mixed read/write operations
static void BM_Transaction_MixedOperations(benchmark::State& state) {
    auto db = std::make_unique<mock_transaction_database>();
    db->connect("");

    query_builder qb_select(database_types::postgres);
    auto select_query = qb_select.select({"*"})
                                 .from("users")
                                 .where("id", "=", 123)
                                 .build();

    query_builder qb_update(database_types::postgres);
    std::map<std::string, database_value> update_data = {
        {"status", "active"}
    };
    auto update_query = qb_update.update(update_data)
                                 .where("id", "=", 123)
                                 .build();

    for (auto _ : state) {
        db->select_query(select_query);
        db->update_query(update_query);
    }
}
BENCHMARK(BM_Transaction_MixedOperations);
