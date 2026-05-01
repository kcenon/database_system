// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file error_handling.cpp
 * @brief Demonstrates the Result<T> error handling pattern used throughout
 *        the database_system API.
 *
 * This example shows how to:
 * - Use VoidResult for operations that either succeed or fail
 * - Use Result<T> for operations that return data on success
 * - Inspect error messages from failed operations
 * - Chain operations with early-return on failure
 * - Handle connection failures, query failures, and data validation
 *
 * Note: Parts of this example intentionally trigger errors to demonstrate
 * the error handling paths. A running PostgreSQL server is required for
 * full execution, but the patterns are useful even without one.
 */

#include <iostream>
#include <memory>
#include <string>
#include <variant>

#include <kcenon/database/database_manager.h>
#include <kcenon/database/core/database_context.h>

using namespace database;

/**
 * @brief Helper to print a Result error.
 *
 * The error type returned by Result<T>::error() is kcenon::common::error_info,
 * which contains code, message, and module fields.
 */
static void print_error(const std::string& operation, const kcenon::common::error_info& err)
{
	std::cerr << "  [ERROR] " << operation << ": " << err.message << std::endl;
}

/**
 * @brief Demonstrates handling a connection failure gracefully.
 */
static void demonstrate_connection_failure()
{
	std::cout << "\n--- Connection failure handling ---" << std::endl;

	auto context = std::make_shared<database_context>();
	auto db = std::make_shared<database_manager>(context);
	db->set_mode(database_types::postgres);

	// Intentionally use wrong credentials
	auto result = db->connect_result(
		"host=localhost port=9999 dbname=nonexistent user=nobody password=wrong");

	if (result.is_ok())
	{
		std::cout << "  Connected (unexpected)" << std::endl;
		db->disconnect_result();
	}
	else
	{
		print_error("connect", result.error());
		std::cout << "  Connection failure handled gracefully" << std::endl;
	}
}

/**
 * @brief Demonstrates handling query errors with Result<T>.
 */
static void demonstrate_query_errors(std::shared_ptr<database_manager> db)
{
	std::cout << "\n--- Query error handling ---" << std::endl;

	// 1. Execute invalid SQL
	auto bad_sql = db->execute_query_result("THIS IS NOT VALID SQL");
	if (!bad_sql.is_ok())
	{
		print_error("bad SQL", bad_sql.error());
	}

	// 2. Reference a non-existent table
	auto missing_table = db->select_query_result("SELECT * FROM nonexistent_table_xyz");
	if (!missing_table.is_ok())
	{
		print_error("missing table", missing_table.error());
	}

	// 3. Insert with constraint violation (duplicate primary key)
	db->create_query_result(R"(
		CREATE TABLE IF NOT EXISTS error_demo (
			id   INTEGER PRIMARY KEY,
			name VARCHAR(50) NOT NULL
		)
	)");
	db->execute_query_result("DELETE FROM error_demo");
	db->execute_query_result("INSERT INTO error_demo (id, name) VALUES (1, 'first')");

	auto duplicate = db->execute_query_result(
		"INSERT INTO error_demo (id, name) VALUES (1, 'duplicate')");
	if (!duplicate.is_ok())
	{
		print_error("duplicate key", duplicate.error());
		std::cout << "  Constraint violation handled" << std::endl;
	}
}

/**
 * @brief Demonstrates chaining operations with early return on failure.
 */
static bool demonstrate_chained_operations(std::shared_ptr<database_manager> db)
{
	std::cout << "\n--- Chained operations with early return ---" << std::endl;

	// Step 1: Create table
	auto step1 = db->create_query_result(R"(
		CREATE TABLE IF NOT EXISTS chain_demo (
			id    SERIAL PRIMARY KEY,
			value INTEGER NOT NULL
		)
	)");
	if (!step1.is_ok())
	{
		print_error("step 1 (create table)", step1.error());
		return false;
	}
	std::cout << "  Step 1: Table created" << std::endl;

	// Step 2: Insert data
	auto step2 = db->execute_query_result(
		"INSERT INTO chain_demo (value) VALUES (42)");
	if (!step2.is_ok())
	{
		print_error("step 2 (insert)", step2.error());
		return false;
	}
	std::cout << "  Step 2: Data inserted" << std::endl;

	// Step 3: Read back
	auto step3 = db->select_query_result("SELECT id, value FROM chain_demo");
	if (!step3.is_ok())
	{
		print_error("step 3 (select)", step3.error());
		return false;
	}
	std::cout << "  Step 3: Read " << step3.value().size() << " row(s)" << std::endl;

	// Step 4: Verify data
	if (step3.value().empty())
	{
		std::cerr << "  [ERROR] No rows returned" << std::endl;
		return false;
	}
	std::cout << "  Step 4: Data verified" << std::endl;

	return true;
}

/**
 * @brief Demonstrates using last_error() for additional diagnostics.
 */
static void demonstrate_last_error(std::shared_ptr<database_manager> db)
{
	std::cout << "\n--- last_error() diagnostics ---" << std::endl;

	// Trigger an error
	db->execute_query_result("SELECT * FROM __does_not_exist__");

	auto err = db->last_error();
	if (!err.empty())
	{
		std::cout << "  last_error(): " << err << std::endl;
	}
	else
	{
		std::cout << "  last_error() returned empty (backend may not populate it)"
				  << std::endl;
	}
}

int main()
{
	std::cout << "=== error_handling example ===" << std::endl;

	// Demonstrate connection failure (does not need a running DB)
	demonstrate_connection_failure();

	// For the remaining demos, attempt a real connection
	auto context = std::make_shared<database_context>();
	auto db = std::make_shared<database_manager>(context);
	db->set_mode(database_types::postgres);

	std::string connection_string
		= "host=localhost port=5432 dbname=example_db user=user password=password";

	auto connect = db->connect_result(connection_string);
	if (!connect.is_ok())
	{
		std::cout << "\nSkipping remaining demos (no database connection available)"
				  << std::endl;
		std::cout << "=== error_handling example completed ===" << std::endl;
		return 0;
	}

	demonstrate_query_errors(db);
	demonstrate_chained_operations(db);
	demonstrate_last_error(db);

	// Connection info for diagnostics
	std::cout << "\n--- Connection info ---" << std::endl;
	auto info = db->connection_info();
	for (const auto& [key, value] : info)
	{
		std::cout << "  " << key << " = " << value << std::endl;
	}

	db->disconnect_result();
	std::cout << "\nDisconnected" << std::endl;

	std::cout << "=== error_handling example completed ===" << std::endl;
	return 0;
}
