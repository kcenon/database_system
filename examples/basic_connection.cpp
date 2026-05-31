// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file basic_connection.cpp
 * @brief Demonstrates basic database connection and simple query execution.
 *
 * This example shows how to:
 * - Create a database_context and database_manager
 * - Configure the database backend (PostgreSQL)
 * - Connect to a database using a connection string
 * - Execute a simple SQL query
 * - Retrieve and display results
 * - Disconnect cleanly
 *
 * Prerequisites:
 * - A running PostgreSQL server (or adjust for your backend)
 * - Update the connection string below with valid credentials
 */

#include <iostream>
#include <memory>
#include <string>
#include <variant>

#include <kcenon/database/database_manager.h>
#include <kcenon/database/core/database_context.h>

using namespace kcenon::database;

int main()
{
	std::cout << "=== basic_connection example ===" << std::endl;

	// Step 1: Create a dependency injection context.
	// database_context manages shared components such as the performance
	// monitor and entity manager.
	auto context = std::make_shared<database_context>();

	// Step 2: Create the database manager using the context.
	auto db_manager = std::make_shared<database_manager>(context);

	// Step 3: Select the database backend.
	// Available types: database_types::postgres, database_types::sqlite, etc.
	if (!db_manager->set_mode(database_types::postgres))
	{
		std::cerr << "Failed to set database mode to PostgreSQL" << std::endl;
		return 1;
	}
	std::cout << "Database mode set to PostgreSQL" << std::endl;

	// Step 4: Connect to the database.
	// Adjust the connection string to match your environment.
	std::string connection_string
		= "host=localhost port=5432 dbname=example_db user=user password=password";

	std::cout << "Connecting to database..." << std::endl;
	auto connect_result = db_manager->connect_result(connection_string);

	if (!connect_result.is_ok())
	{
		std::cerr << "Connection failed. Make sure PostgreSQL is running and\n"
				  << "the connection string is correct.\n"
				  << "Connection string format:\n"
				  << "  host=<host> port=<port> dbname=<db> user=<user> password=<pass>"
				  << std::endl;
		return 1;
	}
	std::cout << "Connected successfully" << std::endl;

	// Step 5: Create a table (DDL query).
	std::string create_sql = R"(
		CREATE TABLE IF NOT EXISTS greetings (
			id    SERIAL PRIMARY KEY,
			message VARCHAR(200) NOT NULL
		)
	)";

	auto create_result = db_manager->create_query_result(create_sql);
	if (create_result.is_ok())
	{
		std::cout << "Table 'greetings' is ready" << std::endl;
	}
	else
	{
		std::cerr << "Failed to create table" << std::endl;
	}

	// Step 6: Insert a row (DML query).
	auto insert_result = db_manager->execute_query_result(
		"INSERT INTO greetings (message) VALUES ('Hello from database_system!')");

	if (insert_result.is_ok())
	{
		std::cout << "Row inserted" << std::endl;
	}

	// Step 7: Select rows and display results.
	auto select_result
		= db_manager->select_query_result("SELECT id, message FROM greetings ORDER BY id");

	if (select_result.is_ok())
	{
		const auto& rows = select_result.value();
		std::cout << "Query returned " << rows.size() << " row(s):" << std::endl;

		for (const auto& row : rows)
		{
			for (const auto& [column, value] : row)
			{
				std::cout << "  " << column << " = ";
				std::visit([](const auto& v) { std::cout << v; }, value);
				std::cout << std::endl;
			}
		}
	}
	else
	{
		std::cerr << "Select query failed: " << select_result.error().message << std::endl;
	}

	// Step 8: Disconnect.
	db_manager->disconnect_result();
	std::cout << "Disconnected" << std::endl;

	std::cout << "=== basic_connection example completed ===" << std::endl;
	return 0;
}
