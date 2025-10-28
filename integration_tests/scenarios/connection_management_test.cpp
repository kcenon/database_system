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
#include <vector>
#include <future>
#include "framework/system_fixture.h"
#include "framework/test_helpers.h"

using namespace database;
using namespace database::testing;

/**
 * @brief Test suite for connection management scenarios.
 */
class ConnectionManagementTest : public ConnectionPoolFixture
{
};

/**
 * @test Verify connection pool initialization with default configuration.
 */
TEST_F(ConnectionManagementTest, PoolInitializationDefault)
{
	ASSERT_TRUE(pool_created_) << "Connection pool should be created successfully";

	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr) << "Pool should exist";

	auto stats = pool->get_stats();
	EXPECT_GE(stats.total_connections, 2u) << "Pool should have minimum connections";
}

/**
 * @test Verify connection pool initialization with custom configuration.
 */
TEST_F(ConnectionManagementTest, PoolInitializationCustomConfig)
{
	// Remove existing pool from SetUp() first
	connection_pool_manager::instance().remove_pool(database_types::sqlite);

	// Create pool with custom config
	connection_pool_config custom_config;
	custom_config.min_connections = 5;
	custom_config.max_connections = 15;
	custom_config.acquire_timeout = std::chrono::milliseconds(3000);
	custom_config.connection_string = test_db_path_.string();

	bool created = connection_pool_manager::instance().create_pool(
		database_types::sqlite, custom_config);
	ASSERT_TRUE(created) << "Pool with custom config should be created";

	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto stats = pool->get_stats();
	EXPECT_GE(stats.total_connections, 5u) << "Should have custom minimum connections";
}

/**
 * @test Verify successful connection acquisition from pool.
 */
TEST_F(ConnectionManagementTest, ConnectionAcquisitionSuccess)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto conn_result = pool->acquire_connection();
	ASSERT_TRUE(conn_result.is_ok()) << "Should acquire connection successfully";
	auto conn = conn_result.value();
	EXPECT_TRUE(conn->is_healthy()) << "Connection should be healthy";
}

/**
 * @test Verify connection release back to pool.
 */
TEST_F(ConnectionManagementTest, ConnectionReleaseSuccess)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto initial_available = pool->available_connections();

	auto conn_result = pool->acquire_connection();
	ASSERT_TRUE(conn_result.is_ok());
	auto conn = conn_result.value();
	EXPECT_LT(pool->available_connections(), initial_available)
		<< "Available connections should decrease";

	// Explicitly release connection back to pool
	pool->release_connection(conn);

	EXPECT_EQ(pool->available_connections(), initial_available)
		<< "Connection should be returned to pool";
}

/**
 * @test Verify connection pooling and reuse.
 */
TEST_F(ConnectionManagementTest, ConnectionPoolingAndReuse)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	// Acquire and release multiple times
	for (int i = 0; i < 5; ++i) {
		auto conn_result = pool->acquire_connection();
		ASSERT_TRUE(conn_result.is_ok()) << "Iteration " << i;
		pool->release_connection(conn_result.value());
	}

	auto stats = pool->get_stats();
	EXPECT_GE(stats.successful_acquisitions, 5u)
		<< "Should track successful acquisitions";
}

/**
 * @test Verify connection timeout handling.
 */
TEST_F(ConnectionManagementTest, ConnectionTimeoutHandling)
{
	// Create pool with small max connections
	connection_pool_config config;
	config.min_connections = 1;
	config.max_connections = 2;
	config.acquire_timeout = std::chrono::milliseconds(1000);
	config.connection_string = test_db_path_.string();

	connection_pool_manager::instance().remove_pool(database_types::sqlite);
	bool created = connection_pool_manager::instance().create_pool(
		database_types::sqlite, config);
	ASSERT_TRUE(created);

	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	// Acquire all connections
	std::vector<std::shared_ptr<connection_wrapper>> conns;
	for (size_t i = 0; i < config.max_connections; ++i) {
		auto conn_result = pool->acquire_connection();
		if (conn_result.is_ok()) {
			conns.push_back(conn_result.value());
		}
	}

	// Try to acquire with timeout
	PerformanceTimer timer;
	auto timeout_conn_result = pool->acquire_connection();

	// Connection might be null due to timeout
	if (timeout_conn_result.is_err()) {
		EXPECT_GE(timer.Elapsed(), 900) << "Should wait for timeout";
	}
}

/**
 * @test Verify max connections limit enforcement.
 */
TEST_F(ConnectionManagementTest, MaxConnectionsLimitEnforcement)
{
	connection_pool_config config;
	config.min_connections = 2;
	config.max_connections = 5;
	config.acquire_timeout = std::chrono::milliseconds(500);
	config.connection_string = test_db_path_.string();

	connection_pool_manager::instance().remove_pool(database_types::sqlite);
	connection_pool_manager::instance().create_pool(database_types::sqlite, config);

	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	// Try to acquire more than max
	std::vector<std::shared_ptr<connection_wrapper>> conns;
	for (size_t i = 0; i < config.max_connections + 2; ++i) {
		auto conn_result = pool->acquire_connection();
		if (conn_result.is_ok()) {
			conns.push_back(conn_result.value());
		}
	}

	EXPECT_LE(conns.size(), config.max_connections)
		<< "Should not exceed max connections";
}

/**
 * @test Verify connection health checking.
 */
TEST_F(ConnectionManagementTest, ConnectionHealthChecking)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto conn_result = pool->acquire_connection();
	ASSERT_TRUE(conn_result.is_ok());
	auto conn = conn_result.value();
	EXPECT_TRUE(conn->is_healthy()) << "New connection should be healthy";

	// Mark as unhealthy
	conn->mark_unhealthy();
	EXPECT_FALSE(conn->is_healthy()) << "Connection should be marked unhealthy";
}

/**
 * @test Verify concurrent connection requests.
 */
TEST_F(ConnectionManagementTest, ConcurrentConnectionRequests)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	const int num_threads = 10;
	std::vector<std::future<bool>> futures;

	for (int i = 0; i < num_threads; ++i) {
		futures.push_back(std::async(std::launch::async, [pool]() {
			auto conn_result = pool->acquire_connection();
			if (conn_result.is_err()) {
				return false;
			}
			auto conn = conn_result.value();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			pool->release_connection(conn);
			return true;
		}));
	}

	int successful = 0;
	for (auto& future : futures) {
		if (future.get()) {
			++successful;
		}
	}

	EXPECT_GT(successful, 0) << "At least some threads should acquire connections";
}

/**
 * @test Verify connection string parsing for SQLite.
 */
TEST_F(ConnectionManagementTest, ConnectionStringParsingSQLite)
{
	std::vector<std::string> valid_strings = {
		":memory:",
		"test.db",
		"/path/to/database.db"
	};

	for (const auto& conn_str : valid_strings) {
		EXPECT_TRUE(ValidateConnectionString(conn_str))
			<< "Should accept valid connection string: " << conn_str;
	}
}

/**
 * @test Verify connection metadata tracking.
 */
TEST_F(ConnectionManagementTest, ConnectionMetadataTracking)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto conn_result = pool->acquire_connection();
	ASSERT_TRUE(conn_result.is_ok());
	auto conn = conn_result.value();

	auto initial_time = conn->last_used();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	conn->update_last_used();
	auto updated_time = conn->last_used();

	EXPECT_GT(updated_time, initial_time) << "Last used time should be updated";
}

/**
 * @test Verify idle connection timeout detection.
 */
TEST_F(ConnectionManagementTest, IdleConnectionTimeoutDetection)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto conn_result = pool->acquire_connection();
	ASSERT_TRUE(conn_result.is_ok());
	auto conn = conn_result.value();

	// Check with very short timeout
	auto short_timeout = std::chrono::milliseconds(1);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	EXPECT_TRUE(conn->is_idle_timeout_exceeded(short_timeout))
		<< "Should detect idle timeout";
}

/**
 * @test Verify connection pool statistics tracking.
 */
TEST_F(ConnectionManagementTest, ConnectionPoolStatisticsTracking)
{
	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	// Perform some operations
	auto conn1_result = pool->acquire_connection();
	auto conn2_result = pool->acquire_connection();

	auto stats = pool->get_stats();
	EXPECT_GT(stats.total_connections, 0u) << "Should track total connections";
	EXPECT_GT(stats.active_connections, 0u) << "Should track active connections";
	EXPECT_GE(stats.successful_acquisitions, 2u) << "Should track acquisitions";
}

/**
 * @test Verify connection pool shutdown behavior.
 */
TEST_F(ConnectionManagementTest, ConnectionPoolShutdown)
{
	// Create a temporary pool
	connection_pool_config config;
	config.min_connections = 2;
	config.max_connections = 5;
	config.connection_string = test_db_path_.string();

	connection_pool_manager::instance().remove_pool(database_types::sqlite);
	connection_pool_manager::instance().create_pool(database_types::sqlite, config);

	auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	// Acquire connection before shutdown
	auto conn_result = pool->acquire_connection();
	ASSERT_TRUE(conn_result.is_ok());

	// Shutdown pool
	pool->shutdown();

	// After shutdown, available should be 0
	EXPECT_EQ(pool->available_connections(), 0u)
		<< "Pool should have no available connections after shutdown";
}
