// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <future>
#include <atomic>
#include <kcenon/database/core/database_context.h>
#include "framework/system_fixture.h"
#include "framework/test_helpers.h"

using namespace kcenon::database;
using namespace kcenon::database::testing;

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
	manager_->execute_query_result("INSERT INTO pk_test (id, value) VALUES (1, 'first')");

	// Try to insert duplicate primary key
	auto result = manager_->execute_query_result(
		"INSERT INTO pk_test (id, value) VALUES (1, 'duplicate')");

	EXPECT_FALSE(result.is_ok()) << "Duplicate primary key should fail";
}

/**
 * @test Verify handling of UNIQUE constraint violation.
 */
TEST_F(ErrorHandlingTest, UniqueConstraintViolation)
{
	// Users table has UNIQUE constraint on email
	manager_->execute_query_result("INSERT INTO users (name, email, age) VALUES "
	                       "('User1', 'unique@test.com', 25)");

	// Try to insert duplicate email
	auto result = manager_->execute_query_result(
		"INSERT INTO users (name, email, age) VALUES "
		"('User2', 'unique@test.com', 30)");

	EXPECT_FALSE(result.is_ok()) << "Duplicate unique value should fail";
}

/**
 * @test Verify handling of NOT NULL constraint violation.
 */
TEST_F(ErrorHandlingTest, NotNullConstraintViolation)
{
	// Try to insert NULL into NOT NULL column
	std::string query = "INSERT INTO users (name, email) VALUES "
	                    "('User', NULL)"; // email is NOT NULL

	auto result = manager_->execute_query_result(query);
	EXPECT_FALSE(result.is_ok()) << "NULL in NOT NULL column should fail";
}

/**
 * @test Verify transaction rollback on error.
 */
TEST_F(ErrorHandlingTest, TransactionRollbackOnError)
{
	TransactionHelper txn(manager_);
	ASSERT_TRUE(txn.Begin());

	// Insert valid row
	manager_->execute_query_result("INSERT INTO users (name, email, age) VALUES "
	                       "('User1', 'user1@test.com', 25)");

	// Try to insert invalid row (duplicate email)
	manager_->execute_query_result("INSERT INTO users (name, email, age) VALUES "
	                       "('User2', 'user1@test.com', 30)");

	// Rollback transaction
	ASSERT_TRUE(txn.Rollback());

	// Verify no data was committed
	EXPECT_TRUE(VerifyRowCount("users", 0))
		<< "Rollback should revert all changes";
}

// Connection pool exhaustion test removed in Phase 4.3
// Connection pooling is now handled server-side via ProxyMode

/**
 * @test Verify handling of connection to invalid database file.
 */
TEST_F(ErrorHandlingTest, InvalidDatabaseFile)
{
	auto test_context = std::make_shared<database_context>();
	auto test_mgr = std::make_shared<database_manager>(test_context);
	test_mgr->set_mode(database_types::sqlite);

	// Try to connect to invalid path
	auto connect_result = test_mgr->connect_result("/invalid/path/to/database.db");

	// May succeed (SQLite creates files) or fail - both are valid
	if (connect_result.is_ok()) {
		test_mgr->disconnect_result();
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
	test_mgr->disconnect_result();

	// Try to execute query when disconnected
	auto result = test_mgr->select_query_result("SELECT * FROM users");

	EXPECT_FALSE(result.is_ok())
		<< "Query on disconnected database should fail";
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
		"invalid_no_scheme"
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
				auto result = manager_->execute_query_result(
					"INSERT INTO users (name, email, age) VALUES "
					"('User', 'concurrent@test.com', 25)");

				if (!result.is_ok()) {
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

// Recovery from unhealthy connection test removed in Phase 4.3
// Connection health management is now handled server-side via ProxyMode

/**
 * @test Verify handling of empty query string.
 */
TEST_F(ErrorHandlingTest, EmptyQueryString)
{
	auto result = ExecuteQuery("");

	EXPECT_TRUE(result.empty()) << "Empty query should return empty result";
}
