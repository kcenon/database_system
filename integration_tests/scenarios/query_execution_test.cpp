// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <future>
#include "framework/system_fixture.h"
#include "framework/test_helpers.h"

using namespace kcenon::database;
using namespace kcenon::database::testing;

// Helper function to extract string from variant
inline std::string get_string_value(const core::database_value& val) {
	if (std::holds_alternative<std::string>(val)) {
		return std::get<std::string>(val);
	}
	if (std::holds_alternative<int64_t>(val)) {
		return std::to_string(std::get<int64_t>(val));
	}
	if (std::holds_alternative<double>(val)) {
		return std::to_string(std::get<double>(val));
	}
	if (std::holds_alternative<bool>(val)) {
		return std::get<bool>(val) ? "true" : "false";
	}
	return "";
}

// Helper function to extract int from variant
inline int get_int_value(const core::database_value& val) {
	if (std::holds_alternative<int64_t>(val)) {
		return static_cast<int>(std::get<int64_t>(val));
	}
	if (std::holds_alternative<std::string>(val)) {
		try {
			return std::stoi(std::get<std::string>(val));
		} catch (...) {
			return 0;
		}
	}
	if (std::holds_alternative<double>(val)) {
		return static_cast<int>(std::get<double>(val));
	}
	return 0;
}

// Helper function to extract size_t from variant
inline size_t get_size_t_value(const core::database_value& val) {
	if (std::holds_alternative<int64_t>(val)) {
		return static_cast<size_t>(std::get<int64_t>(val));
	}
	if (std::holds_alternative<std::string>(val)) {
		try {
			return std::stoul(std::get<std::string>(val));
		} catch (...) {
			return 0;
		}
	}
	if (std::holds_alternative<double>(val)) {
		return static_cast<size_t>(std::get<double>(val));
	}
	return 0;
}

/**
 * @brief Test suite for query execution scenarios.
 */
class QueryExecutionTest : public DatabaseSystemFixture
{
};

/**
 * @test Verify simple SELECT query execution.
 */
TEST_F(QueryExecutionTest, SimpleSelectQuery)
{
	InsertTestUsers(3);

	auto result = ExecuteQuery("SELECT * FROM users");
	EXPECT_EQ(result.size(), 3u) << "Should retrieve 3 users";
}

/**
 * @test Verify simple INSERT query execution.
 */
TEST_F(QueryExecutionTest, SimpleInsertQuery)
{
	std::string query = "INSERT INTO users (name, email, age) VALUES "
	                    "('John Doe', 'john@test.com', 30)";
	auto result = manager_->execute_query_result(query);
	ASSERT_TRUE(result.is_ok()) << "Insert should succeed";

	EXPECT_TRUE(VerifyRowCount("users", 1)) << "Table should have 1 row";
}

/**
 * @test Verify simple UPDATE query execution.
 */
TEST_F(QueryExecutionTest, SimpleUpdateQuery)
{
	InsertTestUsers(1);

	std::string update_query = "UPDATE users SET age = 35 WHERE name = 'User0'";
	auto update_result = manager_->execute_query_result(update_query);
	ASSERT_TRUE(update_result.is_ok()) << "Update should succeed";

	auto result = ExecuteQuery("SELECT age FROM users WHERE name = 'User0'");
	ASSERT_FALSE(result.empty());
	EXPECT_EQ(get_string_value(result[0].at("age")), "35") << "Age should be updated";
}

/**
 * @test Verify simple DELETE query execution.
 */
TEST_F(QueryExecutionTest, SimpleDeleteQuery)
{
	InsertTestUsers(3);

	std::string delete_query = "DELETE FROM users WHERE name = 'User1'";
	auto delete_result = manager_->execute_query_result(delete_query);
	ASSERT_TRUE(delete_result.is_ok()) << "Delete should succeed";

	EXPECT_TRUE(VerifyRowCount("users", 2)) << "Should have 2 users remaining";
}

/**
 * @test Verify prepared statement pattern usage.
 */
TEST_F(QueryExecutionTest, PreparedStatementPattern)
{
	// Using parameterized query pattern
	for (int i = 0; i < 5; ++i) {
		std::string query = "INSERT INTO users (name, email, age) VALUES ("
		                   "'User" + std::to_string(i) + "', "
		                   "'user" + std::to_string(i) + "@test.com', "
		                   + std::to_string(25 + i) + ")";
		manager_->execute_query_result(query);
	}

	EXPECT_TRUE(VerifyRowCount("users", 5)) << "Should insert 5 users";
}

/**
 * @test Verify transaction BEGIN/COMMIT flow.
 */
TEST_F(QueryExecutionTest, TransactionBeginCommit)
{
	TransactionHelper txn(manager_);

	ASSERT_TRUE(txn.Begin()) << "Should begin transaction";

	manager_->execute_query_result("INSERT INTO users (name, email, age) VALUES "
	                       "('TxnUser1', 'txn1@test.com', 40)");
	manager_->execute_query_result("INSERT INTO users (name, email, age) VALUES "
	                       "('TxnUser2', 'txn2@test.com', 41)");

	ASSERT_TRUE(txn.Commit()) << "Should commit transaction";

	EXPECT_TRUE(VerifyRowCount("users", 2)) << "Both inserts should be committed";
}

/**
 * @test Verify transaction ROLLBACK flow.
 */
TEST_F(QueryExecutionTest, TransactionRollback)
{
	TransactionHelper txn(manager_);

	ASSERT_TRUE(txn.Begin()) << "Should begin transaction";

	manager_->execute_query_result("INSERT INTO users (name, email, age) VALUES "
	                       "('RollbackUser', 'rollback@test.com', 45)");

	ASSERT_TRUE(txn.Rollback()) << "Should rollback transaction";

	EXPECT_TRUE(VerifyRowCount("users", 0)) << "Insert should be rolled back";
}

/**
 * @test Verify batch insert operations.
 */
TEST_F(QueryExecutionTest, BatchInsertOperations)
{
	const size_t batch_size = 100;
	size_t inserted = InsertTestUsers(batch_size);

	EXPECT_EQ(inserted, batch_size) << "Should insert all users in batch";
	EXPECT_TRUE(VerifyRowCount("users", batch_size));
}

/**
 * @test Verify parameterized query with multiple parameters.
 */
TEST_F(QueryExecutionTest, ParameterizedQueryMultipleParams)
{
	std::string name = "Param User";
	std::string email = "param@test.com";
	int age = 28;

	std::string query = "INSERT INTO users (name, email, age) VALUES ("
	                   "'" + name + "', '" + email + "', " + std::to_string(age) + ")";
	manager_->execute_query_result(query);

	auto result = ExecuteQuery("SELECT * FROM users WHERE email = '" + email + "'");
	ASSERT_FALSE(result.empty());
	EXPECT_EQ(get_string_value(result[0].at("name")), name);
	EXPECT_EQ(get_string_value(result[0].at("age")), std::to_string(age));
}

/**
 * @test Verify result set iteration and access.
 */
TEST_F(QueryExecutionTest, ResultSetIterationAndAccess)
{
	InsertTestUsers(5);

	auto result = ExecuteQuery("SELECT name, email FROM users ORDER BY name");
	ASSERT_EQ(result.size(), 5u);

	for (size_t i = 0; i < result.size(); ++i) {
		EXPECT_FALSE(get_string_value(result[i].at("name")).empty()) << "Name should not be empty";
		EXPECT_FALSE(get_string_value(result[i].at("email")).empty()) << "Email should not be empty";
	}
}

/**
 * @test Verify query with WHERE clause filtering.
 */
TEST_F(QueryExecutionTest, QueryWithWhereClause)
{
	// Insert 20 users: ages 20-39 (20 + (i % 50) for i=0..19)
	InsertTestUsers(20);

	auto result = ExecuteQuery("SELECT * FROM users WHERE age >= 30 AND age < 40");
	EXPECT_GT(result.size(), 0u) << "Should find users in age range";

	for (const auto& row : result) {
		int age = get_int_value(row.at("age"));
		EXPECT_GE(age, 30);
		EXPECT_LT(age, 40);
	}
}

/**
 * @test Verify query with ORDER BY clause.
 */
TEST_F(QueryExecutionTest, QueryWithOrderBy)
{
	InsertTestUsers(5);

	auto result = ExecuteQuery("SELECT name FROM users ORDER BY name ASC");
	ASSERT_EQ(result.size(), 5u);

	// Verify ordering
	for (size_t i = 1; i < result.size(); ++i) {
		EXPECT_LE(get_string_value(result[i-1].at("name")), get_string_value(result[i].at("name")))
			<< "Results should be ordered ascending";
	}
}

/**
 * @test Verify concurrent query execution.
 */
TEST_F(QueryExecutionTest, ConcurrentQueryExecution)
{
	InsertTestUsers(10);

	const int num_threads = 5;
	std::vector<std::future<size_t>> futures;

	for (int i = 0; i < num_threads; ++i) {
		futures.push_back(std::async(std::launch::async, [this]() {
			auto result = ExecuteQuery("SELECT COUNT(*) as cnt FROM users");
			if (result.empty()) {
				return 0ul;
			}
			return get_size_t_value(result[0].at("cnt"));
		}));
	}

	for (auto& future : futures) {
		size_t count = future.get();
		EXPECT_EQ(count, 10u) << "Each thread should see all users";
	}
}
