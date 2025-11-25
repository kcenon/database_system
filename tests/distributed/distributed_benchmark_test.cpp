/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file distributed_benchmark_test.cpp
 * @brief Performance benchmarks for distributed system
 */

#include "multi_node_test_framework.h"
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace database;
using namespace database::test;
using namespace database::distributed;

class DistributedBenchmarkTest : public MultiNodeTestBase {
protected:
    void SetUp() override {
        config_.num_primary_nodes = 1;
        config_.num_replica_nodes = 2;
        config_.enable_gateway = false;
        config_.enable_replication = true;
        MultiNodeTestBase::SetUp();

        // Create benchmark table
        execute_on_primary("CREATE TABLE IF NOT EXISTS bench (id INTEGER PRIMARY KEY, data TEXT)");
    }

    void TearDown() override {
        execute_on_primary("DROP TABLE IF EXISTS bench");
        MultiNodeTestBase::TearDown();
    }
};

/**
 * Test: Throughput benchmark - single threaded
 */
TEST_F(DistributedBenchmarkTest, SingleThreadedThroughput) {
    constexpr int NUM_OPERATIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    int success_count = 0;
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        std::string query = "INSERT INTO bench VALUES (" +
                          std::to_string(i) + ", 'data_" +
                          std::to_string(i) + "')";
        auto result = execute_on_primary(query);
        if (result.rows_affected > 0) {
            success_count++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double ops_per_second = (success_count * 1000.0) / duration.count();

    std::cout << "Single-threaded throughput:\n";
    std::cout << "  Operations: " << success_count << "/" << NUM_OPERATIONS << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << ops_per_second << " ops/sec\n";

    EXPECT_GT(ops_per_second, 10.0) << "Should achieve minimum throughput";
    EXPECT_GT(success_count, NUM_OPERATIONS * 0.95)
        << "Should have high success rate";
}

/**
 * Test: Throughput benchmark - multi-threaded
 */
TEST_F(DistributedBenchmarkTest, MultiThreadedThroughput) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 100;
    constexpr int TOTAL_OPS = NUM_THREADS * OPS_PER_THREAD;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                int id = t * OPS_PER_THREAD + i;
                std::string query = "INSERT INTO bench VALUES (" +
                                  std::to_string(id) + ", 'data_" +
                                  std::to_string(id) + "')";
                auto result = execute_on_primary(query);
                if (result.rows_affected > 0) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double ops_per_second = (success_count.load() * 1000.0) / duration.count();
    double success_rate = (success_count.load() * 100.0) / TOTAL_OPS;

    std::cout << "Multi-threaded throughput (" << NUM_THREADS << " threads):\n";
    std::cout << "  Successful: " << success_count.load() << "\n";
    std::cout << "  Failed: " << failure_count.load() << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << ops_per_second << " ops/sec\n";
    std::cout << "  Success rate: " << success_rate << "%\n";

    EXPECT_GT(ops_per_second, 50.0) << "Should achieve reasonable concurrent throughput";
    EXPECT_GT(success_rate, 80.0) << "Should have acceptable success rate";
}

/**
 * Test: Read/Write ratio benchmark
 */
TEST_F(DistributedBenchmarkTest, ReadWriteRatioBenchmark) {
    // Pre-populate data
    for (int i = 0; i < 100; ++i) {
        execute_on_primary("INSERT INTO bench VALUES (" +
                         std::to_string(i) + ", 'data_" +
                         std::to_string(i) + "')");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 50;
    const float READ_RATIO = 0.8f;  // 80% reads, 20% writes

    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                float r = static_cast<float>(std::rand()) / RAND_MAX;
                if (r < READ_RATIO) {
                    // Read operation
                    int id = std::rand() % 100;
                    execute_on_replica("SELECT * FROM bench WHERE id = " + std::to_string(id));
                    read_count++;
                } else {
                    // Write operation
                    int id = 100 + t * OPS_PER_THREAD + i;
                    execute_on_primary("INSERT INTO bench VALUES (" +
                                     std::to_string(id) + ", 'new_data')");
                    write_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int total_ops = read_count.load() + write_count.load();
    double ops_per_second = (total_ops * 1000.0) / duration.count();

    std::cout << "Read/Write ratio benchmark:\n";
    std::cout << "  Reads: " << read_count.load() << "\n";
    std::cout << "  Writes: " << write_count.load() << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << ops_per_second << " ops/sec\n";

    EXPECT_GT(ops_per_second, 50.0) << "Mixed workload should have good throughput";
}

/**
 * Test: Replication lag measurement
 */
TEST_F(DistributedBenchmarkTest, ReplicationLagMeasurement) {
    if (!replication_) {
        GTEST_SKIP() << "Replication not enabled";
    }

    ReplicationLagMeasurer measurer(this);

    // Collect lag samples
    constexpr size_t SAMPLE_COUNT = 20;
    auto stats = measurer.collect_lag_statistics(SAMPLE_COUNT, "lag_test_bench");

    std::cout << "Replication lag statistics (" << stats.sample_count << " samples):\n";
    std::cout << "  Min lag: " << stats.min_lag.count() << " μs\n";
    std::cout << "  Max lag: " << stats.max_lag.count() << " μs\n";
    std::cout << "  Avg lag: " << stats.avg_lag.count() << " μs\n";
    std::cout << "  Median lag: " << stats.median_lag.count() << " μs\n";

    // Verify acceptable lag
    EXPECT_LT(stats.avg_lag.count(), 100000) << "Average lag should be < 100ms";
    EXPECT_GT(stats.sample_count, SAMPLE_COUNT * 0.8)
        << "Should successfully measure most samples";
}

/**
 * Test: Latency distribution
 */
TEST_F(DistributedBenchmarkTest, LatencyDistribution) {
    constexpr int NUM_SAMPLES = 100;
    std::vector<std::chrono::microseconds> latencies;
    latencies.reserve(NUM_SAMPLES);

    // Measure write latencies
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        std::string query = "INSERT INTO bench VALUES (" +
                          std::to_string(i + 1000) + ", 'latency_test')";
        execute_on_primary(query);

        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        latencies.push_back(latency);
    }

    // Calculate percentiles
    std::sort(latencies.begin(), latencies.end());

    auto p50 = latencies[NUM_SAMPLES * 50 / 100];
    auto p95 = latencies[NUM_SAMPLES * 95 / 100];
    auto p99 = latencies[NUM_SAMPLES * 99 / 100];
    auto min = latencies.front();
    auto max = latencies.back();

    auto sum = std::accumulate(latencies.begin(), latencies.end(),
                               std::chrono::microseconds(0));
    auto avg = sum / NUM_SAMPLES;

    std::cout << "Write latency distribution:\n";
    std::cout << "  Min: " << min.count() << " μs\n";
    std::cout << "  P50: " << p50.count() << " μs\n";
    std::cout << "  P95: " << p95.count() << " μs\n";
    std::cout << "  P99: " << p99.count() << " μs\n";
    std::cout << "  Max: " << max.count() << " μs\n";
    std::cout << "  Avg: " << avg.count() << " μs\n";

    EXPECT_LT(p50.count(), 50000) << "P50 latency should be < 50ms";
    EXPECT_LT(p99.count(), 200000) << "P99 latency should be < 200ms";
}

/**
 * Test: Sustained load test
 */
TEST_F(DistributedBenchmarkTest, SustainedLoadTest) {
    constexpr int DURATION_SECONDS = 10;
    constexpr int NUM_THREADS = 5;

    std::atomic<bool> stop{false};
    std::atomic<int> total_ops{0};
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            int local_ops = 0;
            while (!stop.load()) {
                int id = t * 10000 + local_ops;
                std::string query = "INSERT INTO bench VALUES (" +
                                  std::to_string(id) + ", 'sustained')";
                execute_on_primary(query);
                local_ops++;
                total_ops++;

                // Small delay to simulate realistic workload
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SECONDS));
    stop = true;

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    double ops_per_second = total_ops.load() / static_cast<double>(duration.count());

    std::cout << "Sustained load test (" << DURATION_SECONDS << " seconds):\n";
    std::cout << "  Total operations: " << total_ops.load() << "\n";
    std::cout << "  Average throughput: " << ops_per_second << " ops/sec\n";

    EXPECT_GT(ops_per_second, 10.0) << "Should maintain reasonable throughput";
    EXPECT_TRUE(cluster_->is_healthy()) << "Cluster should remain healthy";
}

/**
 * Test: Load balancing effectiveness
 */
TEST_F(DistributedBenchmarkTest, LoadBalancingEffectiveness) {
    auto replicas = get_replica_node_ids();
    if (replicas.size() < 2) {
        GTEST_SKIP() << "Need at least 2 replicas";
    }

    // Record initial query counts
    std::map<std::string, uint64_t> initial_counts;
    for (const auto& replica_id : replicas) {
        initial_counts[replica_id] = cluster_->get_node_query_count(replica_id);
    }

    // Execute many read queries
    constexpr int NUM_READS = 100;
    for (int i = 0; i < NUM_READS; ++i) {
        execute_on_replica("SELECT * FROM bench LIMIT 1");
    }

    // Record final query counts
    std::map<std::string, uint64_t> final_counts;
    for (const auto& replica_id : replicas) {
        final_counts[replica_id] = cluster_->get_node_query_count(replica_id);
    }

    // Calculate distribution
    std::cout << "Load distribution across replicas:\n";
    for (const auto& replica_id : replicas) {
        uint64_t queries = final_counts[replica_id] - initial_counts[replica_id];
        double percentage = (queries * 100.0) / NUM_READS;
        std::cout << "  " << replica_id << ": " << queries
                  << " queries (" << percentage << "%)\n";
    }

    // In ideal round-robin: each replica gets ~50%
    // Verify distribution is reasonable (not all to one node)
    for (const auto& replica_id : replicas) {
        uint64_t queries = final_counts[replica_id] - initial_counts[replica_id];
        EXPECT_GT(queries, 0) << "Each replica should handle some queries";
    }
}

/**
 * Test: Concurrent read performance
 */
TEST_F(DistributedBenchmarkTest, ConcurrentReadPerformance) {
    // Pre-populate data
    for (int i = 0; i < 100; ++i) {
        execute_on_primary("INSERT INTO bench VALUES (" +
                         std::to_string(i + 2000) + ", 'read_test')");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    constexpr int NUM_THREADS = 20;
    constexpr int READS_PER_THREAD = 50;

    std::atomic<int> successful_reads{0};
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < READS_PER_THREAD; ++i) {
                int id = 2000 + (std::rand() % 100);
                auto result = execute_on_replica(
                    "SELECT * FROM bench WHERE id = " + std::to_string(id));
                if (!result.rows.empty() || result.rows_affected >= 0) {
                    successful_reads++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int total_reads = NUM_THREADS * READS_PER_THREAD;
    double reads_per_second = (successful_reads.load() * 1000.0) / duration.count();
    double success_rate = (successful_reads.load() * 100.0) / total_reads;

    std::cout << "Concurrent read performance:\n";
    std::cout << "  Successful reads: " << successful_reads.load() << "/" << total_reads << "\n";
    std::cout << "  Duration: " << duration.count() << " ms\n";
    std::cout << "  Throughput: " << reads_per_second << " reads/sec\n";
    std::cout << "  Success rate: " << success_rate << "%\n";

    EXPECT_GT(reads_per_second, 100.0) << "Should achieve high read throughput";
    EXPECT_GT(success_rate, 90.0) << "Should have high success rate";
}
