/**
 * @file transaction_bench.cpp
 * @brief Transaction performance benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include "database/database_manager.h"
#include "database/query_builder.h"
#include <memory>

using namespace database_module;

// Mock database manager for transaction benchmarks
class mock_database : public database_base {
public:
    bool connect() override { return true; }
    bool disconnect() override { return true; }
    bool is_connected() const override { return true; }

    bool execute(const std::string&) override {
        // Simulate small execution overhead
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        return true;
    }

    bool begin_transaction() override {
        transaction_active = true;
        return true;
    }

    bool commit() override {
        transaction_active = false;
        return true;
    }

    bool rollback() override {
        transaction_active = false;
        return true;
    }

    std::string get_last_error() const override { return ""; }

private:
    bool transaction_active = false;
};

// Benchmark transaction begin/commit cycle
static void BM_Transaction_BeginCommit(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    for (auto _ : state) {
        db->begin_transaction();
        db->commit();
    }
}
BENCHMARK(BM_Transaction_BeginCommit);

// Benchmark transaction with single query
static void BM_Transaction_SingleQuery(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    query_builder qb;
    auto query = qb.insert_into("users")
                   .values({{"name", "Test"}, {"email", "test@example.com"}})
                   .build();

    for (auto _ : state) {
        db->begin_transaction();
        db->execute(query);
        db->commit();
    }
}
BENCHMARK(BM_Transaction_SingleQuery);

// Benchmark transaction with multiple queries
static void BM_Transaction_MultipleQueries(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    int num_queries = state.range(0);
    std::vector<std::string> queries;

    for (int i = 0; i < num_queries; ++i) {
        query_builder qb;
        queries.push_back(
            qb.insert_into("users")
              .values({{"name", "User" + std::to_string(i)}})
              .build()
        );
    }

    for (auto _ : state) {
        db->begin_transaction();
        for (const auto& query : queries) {
            db->execute(query);
        }
        db->commit();
    }

    state.SetItemsProcessed(state.iterations() * num_queries);
}
BENCHMARK(BM_Transaction_MultipleQueries)->Arg(5)->Arg(10)->Arg(50)->Arg(100);

// Benchmark transaction rollback
static void BM_Transaction_Rollback(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    query_builder qb;
    auto query = qb.insert_into("users")
                   .values({{"name", "Test"}})
                   .build();

    for (auto _ : state) {
        db->begin_transaction();
        db->execute(query);
        db->rollback();
    }
}
BENCHMARK(BM_Transaction_Rollback);

// Benchmark nested transaction handling
static void BM_Transaction_Nested(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    for (auto _ : state) {
        db->begin_transaction();
        db->execute("INSERT INTO users (name) VALUES ('Outer')");

        // Simulate savepoint or nested transaction
        db->execute("SAVEPOINT sp1");
        db->execute("INSERT INTO users (name) VALUES ('Inner')");
        db->execute("RELEASE SAVEPOINT sp1");

        db->commit();
    }
}
BENCHMARK(BM_Transaction_Nested);

// Benchmark transaction with mixed read/write operations
static void BM_Transaction_MixedOperations(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    query_builder qb_select;
    auto select_query = qb_select.select({"*"})
                                 .from("users")
                                 .where("id = 123")
                                 .build();

    query_builder qb_update;
    auto update_query = qb_update.update("users")
                                 .set({{"last_login", "NOW()"}})
                                 .where("id = 123")
                                 .build();

    for (auto _ : state) {
        db->begin_transaction();
        db->execute(select_query);  // Read
        db->execute(update_query);  // Write
        db->commit();
    }
}
BENCHMARK(BM_Transaction_MixedOperations);

// Benchmark transaction isolation overhead
static void BM_Transaction_IsolationOverhead(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    // Simulate setting isolation level
    const std::string set_isolation = "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE";

    query_builder qb;
    auto query = qb.select({"*"}).from("users").build();

    for (auto _ : state) {
        db->execute(set_isolation);
        db->begin_transaction();
        db->execute(query);
        db->commit();
    }
}
BENCHMARK(BM_Transaction_IsolationOverhead);

// Benchmark batch insert within transaction
static void BM_Transaction_BatchInsert(benchmark::State& state) {
    auto db = std::make_unique<mock_database>();
    db->connect();

    int batch_size = state.range(0);

    for (auto _ : state) {
        db->begin_transaction();

        for (int i = 0; i < batch_size; ++i) {
            query_builder qb;
            auto query = qb.insert_into("events")
                           .values({
                               {"event_type", "test"},
                               {"user_id", std::to_string(i)},
                               {"timestamp", "NOW()"}
                           })
                           .build();
            db->execute(query);
        }

        db->commit();
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * 100); // Approx 100 bytes per insert
}
BENCHMARK(BM_Transaction_BatchInsert)->Arg(10)->Arg(100)->Arg(1000);
