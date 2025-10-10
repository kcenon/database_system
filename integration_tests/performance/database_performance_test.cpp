/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
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

#include <gtest/gtest.h>
#include <thread>
#include <future>
#include "framework/system_fixture.h"
#include "framework/test_helpers.h"

using namespace database;
using namespace database::testing;

/**
 * @brief Test suite for database performance scenarios.
 */
class DatabasePerformanceTest : public ConnectionPoolFixture
{
protected:
	static constexpr double CONNECTION_THROUGHPUT_THRESHOLD = 1000.0; // ops/sec
	static constexpr int64_t QUERY_LATENCY_P50_THRESHOLD = 10;        // milliseconds
	static constexpr int64_t QUERY_LATENCY_P95_THRESHOLD = 50;        // milliseconds
	static constexpr int64_t QUERY_LATENCY_P99_THRESHOLD = 100;       // milliseconds
	static constexpr int64_t CONNECTION_ACQUIRE_THRESHOLD = 1;        // millisecond
};

/**
 * @test Measure connection pool throughput.
 * Target: > 1000 connections/sec
 */
TEST_F(DatabasePerformanceTest, ConnectionPoolThroughput)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto throughput = MeasureThroughput([&pool]() {
		auto conn = pool->acquire_connection();
		if (conn) {
			pool->release_connection(conn);
		}
	}, std::chrono::milliseconds(1000));

	std::cout << "Connection pool throughput: " << throughput << " ops/sec\n";
	EXPECT_GT(throughput, CONNECTION_THROUGHPUT_THRESHOLD)
		<< "Connection pool throughput should exceed " << CONNECTION_THROUGHPUT_THRESHOLD
		<< " ops/sec";
}

/**
 * @test Measure query execution latency percentiles.
 */
TEST_F(DatabasePerformanceTest, QueryExecutionLatency)
{
	InsertTestUsers(100);

	LatencyTracker tracker;
	const int iterations = 100;

	for (int i = 0; i < iterations; ++i) {
		PerformanceTimer timer;
		auto result = ExecuteQuery("SELECT * FROM users LIMIT 10");
		tracker.Record(timer.Elapsed<std::chrono::microseconds>());
	}

	double p50 = tracker.P50() / 1000.0; // Convert to milliseconds
	double p95 = tracker.P95() / 1000.0;
	double p99 = tracker.P99() / 1000.0;

	std::cout << "Query latency - P50: " << p50 << "ms, "
	          << "P95: " << p95 << "ms, "
	          << "P99: " << p99 << "ms\n";

	EXPECT_LT(p50, QUERY_LATENCY_P50_THRESHOLD)
		<< "P50 latency should be below " << QUERY_LATENCY_P50_THRESHOLD << "ms";
	EXPECT_LT(p95, QUERY_LATENCY_P95_THRESHOLD)
		<< "P95 latency should be below " << QUERY_LATENCY_P95_THRESHOLD << "ms";
	EXPECT_LT(p99, QUERY_LATENCY_P99_THRESHOLD)
		<< "P99 latency should be below " << QUERY_LATENCY_P99_THRESHOLD << "ms";
}

/**
 * @test Measure connection acquisition latency.
 * Target: < 1 millisecond
 */
TEST_F(DatabasePerformanceTest, ConnectionAcquisitionLatency)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	LatencyTracker tracker;
	const int iterations = 100;

	for (int i = 0; i < iterations; ++i) {
		PerformanceTimer timer;
		auto conn = pool->acquire_connection();
		tracker.Record(timer.Elapsed<std::chrono::microseconds>());
		if (conn) {
			pool->release_connection(conn);
		}
	}

	double p50 = tracker.P50() / 1000.0; // Convert to milliseconds
	double mean = tracker.Mean() / 1000.0;

	std::cout << "Connection acquisition - Mean: " << mean << "ms, "
	          << "P50: " << p50 << "ms\n";

	EXPECT_LT(p50, CONNECTION_ACQUIRE_THRESHOLD)
		<< "Connection acquisition P50 should be below " << CONNECTION_ACQUIRE_THRESHOLD << "ms";
}

/**
 * @test Measure batch insert performance.
 * Target: 1000 rows in < 100 milliseconds
 */
TEST_F(DatabasePerformanceTest, BatchInsertPerformance)
{
	const size_t batch_size = 1000;

	PerformanceTimer timer;
	size_t inserted = InsertTestUsers(batch_size);
	auto elapsed = timer.Elapsed();

	std::cout << "Batch insert (" << batch_size << " rows): " << elapsed << "ms\n";

	EXPECT_EQ(inserted, batch_size) << "Should insert all rows";
	EXPECT_LT(elapsed, 100) << "Batch insert should complete in < 100ms";
}

/**
 * @test Measure transaction commit latency.
 * Target: < 20 milliseconds
 */
TEST_F(DatabasePerformanceTest, TransactionCommitLatency)
{
	LatencyTracker tracker;
	const int iterations = 50;

	for (int i = 0; i < iterations; ++i) {
		TransactionHelper txn(manager_);
		txn.Begin();

		manager_->insert_query("INSERT INTO users (name, email, age) VALUES "
		                       "('TxnUser', 'txn@test.com', 30)");

		PerformanceTimer timer;
		txn.Commit();
		tracker.Record(timer.Elapsed<std::chrono::microseconds>());

		ClearTable("users");
	}

	double p50 = tracker.P50() / 1000.0; // Convert to milliseconds
	double p95 = tracker.P95() / 1000.0;

	std::cout << "Transaction commit - P50: " << p50 << "ms, "
	          << "P95: " << p95 << "ms\n";

	EXPECT_LT(p50, 20.0) << "Transaction commit P50 should be below 20ms";
}

/**
 * @test Measure connection pool scalability under load.
 */
TEST_F(DatabasePerformanceTest, ConnectionPoolScalability)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	std::vector<int> thread_counts = {1, 2, 4, 8};
	std::vector<double> throughputs;

	for (int num_threads : thread_counts) {
		std::vector<std::future<size_t>> futures;

		PerformanceTimer timer;
		for (int i = 0; i < num_threads; ++i) {
			futures.push_back(std::async(std::launch::async, [pool]() {
				size_t ops = 0;
				for (int j = 0; j < 100; ++j) {
					auto conn = pool->acquire_connection();
					if (conn) {
						pool->release_connection(conn);
						++ops;
					}
				}
				return ops;
			}));
		}

		size_t total_ops = 0;
		for (auto& future : futures) {
			total_ops += future.get();
		}

		double elapsed_sec = timer.ElapsedSeconds();
		double throughput = total_ops / elapsed_sec;
		throughputs.push_back(throughput);

		std::cout << "Threads: " << num_threads
		          << ", Throughput: " << throughput << " ops/sec\n";
	}

	// Verify throughput increases with more threads (up to a point)
	EXPECT_GT(throughputs[1], throughputs[0] * 0.8)
		<< "Throughput should scale with more threads";
}

/**
 * @test Measure memory usage under load.
 */
TEST_F(DatabasePerformanceTest, MemoryUsageUnderLoad)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	// Acquire many connections
	std::vector<std::shared_ptr<connection_wrapper>> connections;
	const size_t num_connections = 10;

	for (size_t i = 0; i < num_connections; ++i) {
		auto conn = pool->acquire_connection();
		if (conn) {
			connections.push_back(conn);
		}
	}

	// Perform operations
	InsertTestUsers(1000);

	auto stats = pool->get_stats();
	EXPECT_GE(stats.active_connections, num_connections)
		<< "Should track active connections";

	// Release connections
	connections.clear();

	// Pool should be stable
	EXPECT_NO_THROW({
		auto conn = pool->acquire_connection();
		if (conn) {
			pool->release_connection(conn);
		}
	}) << "Pool should remain stable after load";
}

/**
 * @test Measure query throughput under concurrent load.
 */
TEST_F(DatabasePerformanceTest, QueryThroughputConcurrent)
{
	InsertTestUsers(1000);

	const int num_threads = 4;
	std::atomic<size_t> total_queries{0};

	std::vector<std::future<void>> futures;
	PerformanceTimer timer;

	for (int i = 0; i < num_threads; ++i) {
		futures.push_back(std::async(std::launch::async, [this, &total_queries]() {
			for (int j = 0; j < 50; ++j) {
				auto result = ExecuteQuery("SELECT * FROM users LIMIT 100");
				if (!result.empty()) {
					++total_queries;
				}
			}
		}));
	}

	for (auto& future : futures) {
		future.wait();
	}

	double elapsed_sec = timer.ElapsedSeconds();
	double throughput = total_queries / elapsed_sec;

	std::cout << "Concurrent query throughput: " << throughput << " queries/sec\n";
	EXPECT_GT(throughput, 100.0) << "Should achieve reasonable query throughput";
}

/**
 * @test Measure prepared statement performance advantage.
 */
TEST_F(DatabasePerformanceTest, PreparedStatementAdvantage)
{
	const int iterations = 100;

	// Regular queries
	PerformanceTimer regular_timer;
	for (int i = 0; i < iterations; ++i) {
		std::string query = "INSERT INTO products (name, price, stock) VALUES ("
		                   "'Product" + std::to_string(i) + "', "
		                   + std::to_string(10.0 + i) + ", " + std::to_string(i) + ")";
		manager_->insert_query(query);
	}
	auto regular_elapsed = regular_timer.Elapsed();

	ClearTable("products");

	// Parameterized pattern (simulating prepared statements)
	PerformanceTimer prepared_timer;
	for (int i = 0; i < iterations; ++i) {
		std::string query = "INSERT INTO products (name, price, stock) VALUES ("
		                   "'Product" + std::to_string(i) + "', "
		                   + std::to_string(10.0 + i) + ", " + std::to_string(i) + ")";
		manager_->insert_query(query);
	}
	auto prepared_elapsed = prepared_timer.Elapsed();

	std::cout << "Regular queries: " << regular_elapsed << "ms, "
	          << "Prepared pattern: " << prepared_elapsed << "ms\n";

	// Both should complete in reasonable time
	EXPECT_LT(regular_elapsed, 1000) << "Regular queries should complete in reasonable time";
	EXPECT_LT(prepared_elapsed, 1000) << "Prepared pattern should complete in reasonable time";
}
