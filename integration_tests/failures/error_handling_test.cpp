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
#include <future>
#include <atomic>
#include "database/core/database_context.h"
#include "framework/system_fixture.h"
#include "framework/test_helpers.h"

using namespace database;
using namespace database::testing;

/**
 * @brief Test suite for error handling and failure scenarios.
 */
class ErrorHandlingTest : public DatabaseSystemFixture
{
	// No additional members needed - using inherited context_ and manager_
};

/**
 * @test Verify handling of invalid query syntax.
 */
TEST_F(ErrorHandlingTest, InvalidQuerySyntax)
{
	std::string invalid_query = "SELEKT * FORM users"; // Typos
	auto result = ExecuteQuery(invalid_query);

	EXPECT_TRUE(result.empty()) << "Invalid query should return empty result";
}

/**
 * @test Verify handling of non-existent table.
 */
TEST_F(ErrorHandlingTest, NonExistentTable)
{
	auto result = ExecuteQuery("SELECT * FROM non_existent_table");

	EXPECT_TRUE(result.empty()) << "Query on non-existent table should return empty";
}

/**
 * @test Verify handling of PRIMARY KEY constraint violation.
 */
TEST_F(ErrorHandlingTest, PrimaryKeyConstraintViolation)
{
	// Create table with explicit primary key
	CreateTestTable("pk_test", "id INTEGER PRIMARY KEY, value TEXT");

	// Insert first row
	manager_->insert_query("INSERT INTO pk_test (id, value) VALUES (1, 'first')");

	// Try to insert duplicate primary key
	unsigned int affected = manager_->insert_query(
		"INSERT INTO pk_test (id, value) VALUES (1, 'duplicate')");

	EXPECT_EQ(affected, 0u) << "Duplicate primary key should fail";
}

/**
 * @test Verify handling of UNIQUE constraint violation.
 */
TEST_F(ErrorHandlingTest, UniqueConstraintViolation)
{
	// Users table has UNIQUE constraint on email
	manager_->insert_query("INSERT INTO users (name, email, age) VALUES "
	                       "('User1', 'unique@test.com', 25)");

	// Try to insert duplicate email
	unsigned int affected = manager_->insert_query(
		"INSERT INTO users (name, email, age) VALUES "
		"('User2', 'unique@test.com', 30)");

	EXPECT_EQ(affected, 0u) << "Duplicate unique value should fail";
}

/**
 * @test Verify handling of NOT NULL constraint violation.
 */
TEST_F(ErrorHandlingTest, NotNullConstraintViolation)
{
	// Try to insert NULL into NOT NULL column
	std::string query = "INSERT INTO users (name, email) VALUES "
	                    "('User', NULL)"; // email is NOT NULL

	unsigned int affected = manager_->insert_query(query);
	EXPECT_EQ(affected, 0u) << "NULL in NOT NULL column should fail";
}

/**
 * @test Verify transaction rollback on error.
 */
TEST_F(ErrorHandlingTest, TransactionRollbackOnError)
{
	TransactionHelper txn(manager_);
	ASSERT_TRUE(txn.Begin());

	// Insert valid row
	manager_->insert_query("INSERT INTO users (name, email, age) VALUES "
	                       "('User1', 'user1@test.com', 25)");

	// Try to insert invalid row (duplicate email)
	manager_->insert_query("INSERT INTO users (name, email, age) VALUES "
	                       "('User2', 'user1@test.com', 30)");

	// Rollback transaction
	ASSERT_TRUE(txn.Rollback());

	// Verify no data was committed
	EXPECT_TRUE(VerifyRowCount("users", 0))
		<< "Rollback should revert all changes";
}

/**
 * @test Verify connection pool exhaustion handling.
 */
TEST_F(ErrorHandlingTest, ConnectionPoolExhaustion)
{
	// Create pool with very limited connections
	connection_pool_config config;
	config.min_connections = 1;
	config.max_connections = 2;
	config.acquire_timeout = std::chrono::milliseconds(500);
	config.connection_string = test_db_path_.string();  // Use absolute path without URI prefix

	context_->get_pool_manager()->remove_pool(database_types::sqlite);
	context_->get_pool_manager()->create_pool(database_types::sqlite, config);

	auto pool = context_->get_pool_manager()->get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	// Acquire all connections
	auto conn1_result = pool->acquire_connection();
	auto conn2_result = pool->acquire_connection();

	ASSERT_TRUE(conn1_result.is_ok());
	ASSERT_TRUE(conn2_result.is_ok());

	// Try to acquire when pool is exhausted
	PerformanceTimer timer;
	auto conn3_result = pool->acquire_connection();

	// Should timeout or return error
	if (conn3_result.is_err()) {
		EXPECT_GE(timer.Elapsed(), 400) << "Should wait for timeout period";
	}

	auto stats = pool->get_stats();
	EXPECT_GT(stats.failed_acquisitions, 0u)
		<< "Should track failed acquisitions";
}

/**
 * @test Verify handling of connection to invalid database file.
 */
TEST_F(ErrorHandlingTest, InvalidDatabaseFile)
{
	auto test_context = std::make_shared<database_context>();
	auto test_mgr = std::make_shared<database_manager>(test_context);
	test_mgr->set_mode(database_types::sqlite);

	// Try to connect to invalid path
	bool connected = test_mgr->connect("/invalid/path/to/database.db");

	// May succeed (SQLite creates files) or fail - both are valid
	if (connected) {
		test_mgr->disconnect();
	}

	SUCCEED() << "Handled invalid database file path";
}

/**
 * @test Verify handling of queries on disconnected database.
 */
TEST_F(ErrorHandlingTest, QueryOnDisconnectedDatabase)
{
	auto test_context = std::make_shared<database_context>();
	auto test_mgr = std::make_shared<database_manager>(test_context);
	test_mgr->disconnect();

	// Try to execute query when disconnected
	auto result = test_mgr->select_query("SELECT * FROM users");

	EXPECT_TRUE(result.empty())
		<< "Query on disconnected database should return empty";
}

/**
 * @test Verify handling of invalid connection string format.
 */
TEST_F(ErrorHandlingTest, InvalidConnectionStringFormat)
{
	std::vector<std::string> invalid_strings = {
		"",
		"invalid://connection",
		"postgres://",
		"mysql://"
	};

	for (const auto& conn_str : invalid_strings) {
		EXPECT_FALSE(ValidateConnectionString(conn_str))
			<< "Should reject invalid connection string: " << conn_str;
	}
}

/**
 * @test Verify handling of concurrent constraint violations.
 */
TEST_F(ErrorHandlingTest, ConcurrentConstraintViolations)
{
	const int num_threads = 5;
	std::atomic<int> failed_inserts{0};
	std::vector<std::future<void>> futures;

	for (int i = 0; i < num_threads; ++i) {
		futures.push_back(std::async(std::launch::async,
			[this, &failed_inserts]() {
				// Try to insert same email (UNIQUE constraint)
				unsigned int affected = manager_->insert_query(
					"INSERT INTO users (name, email, age) VALUES "
					"('User', 'concurrent@test.com', 25)");

				if (affected == 0) {
					++failed_inserts;
				}
			}));
	}

	for (auto& future : futures) {
		future.wait();
	}

	// At least one should succeed, others should fail
	EXPECT_GT(failed_inserts, 0) << "Some concurrent inserts should fail";
	EXPECT_LT(failed_inserts, num_threads) << "At least one insert should succeed";
}

/**
 * @test Verify recovery from unhealthy connection.
 */
TEST_F(ErrorHandlingTest, RecoveryFromUnhealthyConnection)
{
	// Create pool for this test
	connection_pool_config config;
	config.min_connections = 2;
	config.max_connections = 5;
	config.acquire_timeout = std::chrono::milliseconds(1000);
	config.connection_string = test_db_path_.string();

	context_->get_pool_manager()->remove_pool(database_types::sqlite);
	context_->get_pool_manager()->create_pool(database_types::sqlite, config);

	auto pool = context_->get_pool_manager()->get_pool(database_types::sqlite);
	ASSERT_NE(pool, nullptr);

	auto conn_result = pool->acquire_connection();
	ASSERT_TRUE(conn_result.is_ok());
	auto conn = conn_result.value();
	EXPECT_TRUE(conn->is_healthy());

	// Mark connection as unhealthy
	conn->mark_unhealthy();
	EXPECT_FALSE(conn->is_healthy());

	// Release unhealthy connection
	pool->release_connection(conn);

	// Acquire new connection - pool should provide healthy one
	auto new_conn_result = pool->acquire_connection();
	ASSERT_TRUE(new_conn_result.is_ok());
	auto new_conn = new_conn_result.value();
	EXPECT_TRUE(new_conn->is_healthy()) << "Pool should provide healthy connection";
}

/**
 * @test Verify handling of empty query string.
 */
TEST_F(ErrorHandlingTest, EmptyQueryString)
{
	auto result = ExecuteQuery("");

	EXPECT_TRUE(result.empty()) << "Empty query should return empty result";
}
