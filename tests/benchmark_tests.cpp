/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2021, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include <sstream>
#include <vector>
#include <thread>

#include "../database_manager.h"
#include "../postgres_manager.h"
#include "../database_types.h"
#include <container.h>

using namespace database;
using namespace container_module;

// Global flag to check if PostgreSQL is available
static bool g_postgresql_available = false;

// Initialize database connection for benchmarks
class DatabaseBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        if (!g_postgresql_available) {
            state.SkipWithError("PostgreSQL not available");
            return;
        }
        
        auto& db = database_manager::handle();
        
        // Create benchmark table if not exists
        db.create_query("DROP TABLE IF EXISTS benchmark_table");
        db.create_query(
            "CREATE TABLE benchmark_table ("
            "id SERIAL PRIMARY KEY,"
            "name VARCHAR(255) NOT NULL,"
            "age INTEGER,"
            "email VARCHAR(255),"
            "score DOUBLE PRECISION,"
            "active BOOLEAN DEFAULT true,"
            "data TEXT,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        );
        
        // Create indexes for benchmark
        db.create_query("CREATE INDEX idx_benchmark_name ON benchmark_table(name)");
        db.create_query("CREATE INDEX idx_benchmark_age ON benchmark_table(age)");
        db.create_query("CREATE INDEX idx_benchmark_email ON benchmark_table(email)");
    }
    
    void TearDown(const ::benchmark::State& state) override {
        if (g_postgresql_available) {
            auto& db = database_manager::handle();
            db.create_query("DROP TABLE IF EXISTS benchmark_table");
        }
    }
};

// Helper to generate random strings
std::string GenerateRandomString(size_t length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += alphanum[dis(gen)];
    }
    return result;
}

// Connection benchmarks
static void BM_DatabaseConnection(benchmark::State& state) {
    for (auto _ : state) {
        auto& db = database_manager::handle();
        db.set_mode(database_types::postgres);
        
        benchmark::DoNotOptimize(
            db.connect("host=localhost port=5432 dbname=postgres user=postgres")
        );
        
        db.disconnect();
    }
}
BENCHMARK(BM_DatabaseConnection);

// Singleton access benchmark
static void BM_SingletonAccess(benchmark::State& state) {
    for (auto _ : state) {
        auto& db = database_manager::handle();
        benchmark::DoNotOptimize(&db);
    }
}
BENCHMARK(BM_SingletonAccess);

// Insert benchmarks
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_InsertSingleRow)(benchmark::State& state) {
    auto& db = database_manager::handle();
    int counter = 0;
    
    for (auto _ : state) {
        std::string name = "User" + std::to_string(counter++);
        std::string email = name + "@example.com";
        
        auto rows = db.insert_query(
            "INSERT INTO benchmark_table (name, age, email, score) VALUES ('" +
            name + "', " + std::to_string(25 + (counter % 50)) + ", '" +
            email + "', " + std::to_string(50.0 + (counter % 50)) + ")"
        );
        
        benchmark::DoNotOptimize(rows);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_InsertSingleRow);

// Batch insert benchmark
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_InsertBatch)(benchmark::State& state) {
    auto& db = database_manager::handle();
    const int batch_size = state.range(0);
    int counter = 0;
    
    for (auto _ : state) {
        std::stringstream query;
        query << "INSERT INTO benchmark_table (name, age, email, score) VALUES ";
        
        for (int i = 0; i < batch_size; ++i) {
            if (i > 0) query << ", ";
            
            std::string name = "BatchUser" + std::to_string(counter++);
            query << "('" << name << "', "
                  << (20 + (counter % 60)) << ", '"
                  << name << "@batch.com', "
                  << (60.0 + (counter % 40)) << ")";
        }
        
        auto rows = db.insert_query(query.str());
        benchmark::DoNotOptimize(rows);
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_InsertBatch)
    ->RangeMultiplier(10)
    ->Range(1, 1000);

// Update benchmarks
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_UpdateByPrimaryKey)(benchmark::State& state) {
    auto& db = database_manager::handle();
    
    // Pre-populate data
    for (int i = 0; i < 1000; ++i) {
        db.insert_query(
            "INSERT INTO benchmark_table (name, age, email) VALUES "
            "('UpdateUser" + std::to_string(i) + "', " + 
            std::to_string(20 + (i % 60)) + ", 'update" + 
            std::to_string(i) + "@test.com')"
        );
    }
    
    int id = 1;
    for (auto _ : state) {
        auto rows = db.update_query(
            "UPDATE benchmark_table SET age = age + 1 WHERE id = " + 
            std::to_string(id++)
        );
        
        if (id > 1000) id = 1;
        benchmark::DoNotOptimize(rows);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_UpdateByPrimaryKey);

// Delete benchmarks
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_DeleteByPrimaryKey)(benchmark::State& state) {
    auto& db = database_manager::handle();
    
    // Pre-populate more data than we'll delete
    for (int i = 0; i < 10000; ++i) {
        db.insert_query(
            "INSERT INTO benchmark_table (name, age) VALUES "
            "('DeleteUser" + std::to_string(i) + "', " + 
            std::to_string(20 + (i % 60)) + ")"
        );
    }
    
    int id = 1;
    for (auto _ : state) {
        auto rows = db.delete_query(
            "DELETE FROM benchmark_table WHERE id = " + std::to_string(id++)
        );
        
        benchmark::DoNotOptimize(rows);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_DeleteByPrimaryKey);

// Select benchmarks
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_SelectByPrimaryKey)(benchmark::State& state) {
    auto& db = database_manager::handle();
    
    // Pre-populate data
    for (int i = 0; i < 1000; ++i) {
        db.insert_query(
            "INSERT INTO benchmark_table (name, age, email, score) VALUES "
            "('SelectUser" + std::to_string(i) + "', " + 
            std::to_string(20 + (i % 60)) + ", 'select" + 
            std::to_string(i) + "@test.com', " +
            std::to_string(70.0 + (i % 30)) + ")"
        );
    }
    
    int id = 1;
    for (auto _ : state) {
        auto result = db.select_query(
            "SELECT * FROM benchmark_table WHERE id = " + std::to_string(id++)
        );
        
        if (id > 1000) id = 1;
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_SelectByPrimaryKey);

// Range select benchmark
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_SelectRange)(benchmark::State& state) {
    auto& db = database_manager::handle();
    const int range_size = state.range(0);
    
    // Pre-populate data
    for (int i = 0; i < 10000; ++i) {
        db.insert_query(
            "INSERT INTO benchmark_table (name, age, email, score) VALUES "
            "('RangeUser" + std::to_string(i) + "', " + 
            std::to_string(20 + (i % 60)) + ", 'range" + 
            std::to_string(i) + "@test.com', " +
            std::to_string(50.0 + (i % 50)) + ")"
        );
    }
    
    for (auto _ : state) {
        auto result = db.select_query(
            "SELECT * FROM benchmark_table WHERE age BETWEEN 25 AND " +
            std::to_string(25 + range_size)
        );
        
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_SelectRange)
    ->RangeMultiplier(2)
    ->Range(1, 32);

// Complex query benchmark
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_ComplexQuery)(benchmark::State& state) {
    auto& db = database_manager::handle();
    
    // Pre-populate data
    for (int i = 0; i < 5000; ++i) {
        db.insert_query(
            "INSERT INTO benchmark_table (name, age, email, score, active) VALUES "
            "('ComplexUser" + std::to_string(i) + "', " + 
            std::to_string(20 + (i % 60)) + ", 'complex" + 
            std::to_string(i) + "@test.com', " +
            std::to_string(40.0 + (i % 60)) + ", " +
            (i % 2 == 0 ? "true" : "false") + ")"
        );
    }
    
    for (auto _ : state) {
        auto result = db.select_query(
            "SELECT name, age, AVG(score) as avg_score, COUNT(*) as count "
            "FROM benchmark_table "
            "WHERE active = true AND age > 30 "
            "GROUP BY name, age "
            "HAVING AVG(score) > 50 "
            "ORDER BY avg_score DESC "
            "LIMIT 100"
        );
        
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_ComplexQuery);

// Result parsing benchmark
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_ResultParsing)(benchmark::State& state) {
    auto& db = database_manager::handle();
    const int result_size = state.range(0);
    
    // Pre-populate data
    for (int i = 0; i < result_size; ++i) {
        db.insert_query(
            "INSERT INTO benchmark_table (name, age, email, score, data) VALUES "
            "('ParseUser" + std::to_string(i) + "', " + 
            std::to_string(20 + (i % 60)) + ", 'parse" + 
            std::to_string(i) + "@test.com', " +
            std::to_string(60.0 + (i % 40)) + ", '" +
            GenerateRandomString(100) + "')"
        );
    }
    
    for (auto _ : state) {
        auto result = db.select_query(
            "SELECT * FROM benchmark_table LIMIT " + std::to_string(result_size)
        );
        
        if (result) {
            auto rows = result->value_array("row");
            int count = 0;
            
            for (const auto& row : rows) {
                if (!row->is_container()) continue;
                
                auto row_container = std::make_unique<container_module::value_container>(row->data());
                auto name = row_container->get_value("name")->to_string();
                auto age = row_container->get_value("age")->to_int();
                auto score = row_container->get_value("score")->to_double();
                auto data = row_container->get_value("data")->to_string();
                
                benchmark::DoNotOptimize(name);
                benchmark::DoNotOptimize(age);
                benchmark::DoNotOptimize(score);
                benchmark::DoNotOptimize(data);
                count++;
            }
            
            benchmark::DoNotOptimize(count);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * result_size);
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_ResultParsing)
    ->RangeMultiplier(10)
    ->Range(10, 10000);

// Transaction benchmark
BENCHMARK_DEFINE_F(DatabaseBenchmarkFixture, BM_Transaction)(benchmark::State& state) {
    auto& db = database_manager::handle();
    const int ops_per_transaction = state.range(0);
    int counter = 0;
    
    for (auto _ : state) {
        db.create_query("BEGIN");
        
        for (int i = 0; i < ops_per_transaction; ++i) {
            db.insert_query(
                "INSERT INTO benchmark_table (name, age) VALUES "
                "('TxUser" + std::to_string(counter++) + "', " + 
                std::to_string(25 + (counter % 50)) + ")"
            );
        }
        
        db.create_query("COMMIT");
    }
    
    state.SetItemsProcessed(state.iterations() * ops_per_transaction);
}
BENCHMARK_REGISTER_F(DatabaseBenchmarkFixture, BM_Transaction)
    ->RangeMultiplier(10)
    ->Range(1, 100);

// Concurrent access benchmark
static void BM_ConcurrentQueries(benchmark::State& state) {
    if (!g_postgresql_available) {
        state.SkipWithError("PostgreSQL not available");
        return;
    }
    
    const int thread_count = state.range(0);
    
    for (auto _ : state) {
        std::vector<std::thread> threads;
        
        auto query_func = [](int thread_id) {
            auto& db = database_manager::handle();
            
            for (int i = 0; i < 10; ++i) {
                auto result = db.select_query(
                    "SELECT COUNT(*) FROM benchmark_table WHERE age > " +
                    std::to_string(20 + thread_id)
                );
                benchmark::DoNotOptimize(result);
            }
        };
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back(query_func, i);
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        state.SetIterationTime(elapsed.count() / 1e6);
    }
    
    state.SetItemsProcessed(state.iterations() * thread_count * 10);
}
BENCHMARK(BM_ConcurrentQueries)
    ->UseManualTime()
    ->RangeMultiplier(2)
    ->Range(1, 8);

// Main function with PostgreSQL check
int main(int argc, char** argv) {
    // Check if PostgreSQL is available
    {
        auto& db = database_manager::handle();
        db.set_mode(database_types::postgres);
        g_postgresql_available = db.connect("host=localhost port=5432 dbname=postgres user=postgres");
        
        if (!g_postgresql_available) {
            std::cerr << "Warning: PostgreSQL not available. Benchmarks will be skipped.\n";
            std::cerr << "Ensure PostgreSQL is running with:\n";
            std::cerr << "  host=localhost port=5432 dbname=postgres user=postgres\n";
        } else {
            std::cout << "PostgreSQL connection successful. Running benchmarks...\n";
        }
    }
    
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
    
    // Cleanup
    if (g_postgresql_available) {
        auto& db = database_manager::handle();
        db.disconnect();
    }
    
    return 0;
}