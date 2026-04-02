// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file connection_pool_demo.cpp
 * @brief Demonstrates multi-threaded database usage with independent
 *        database_manager instances.
 *
 * This example shows how to:
 * - Create multiple database_manager instances for concurrent use
 * - Use separate database_context instances per thread
 * - Execute queries safely from multiple threads
 * - Aggregate results from parallel database operations
 *
 * Design note: database_manager instances are not thread-safe for
 * concurrent operations on the same instance. Each thread should use
 * its own database_manager. The underlying connection pooling is handled
 * server-side (ProxyMode) or by creating separate connections.
 *
 * Prerequisites:
 * - A running PostgreSQL server
 * - Update the connection string below with valid credentials
 */

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <variant>

#include "database/database_manager.h"
#include "database/core/database_context.h"

using namespace database;

static std::mutex cout_mutex;

/**
 * @brief Thread-safe print helper.
 */
static void thread_print(int thread_id, const std::string& msg)
{
	std::lock_guard<std::mutex> lock(cout_mutex);
	std::cout << "  [thread " << thread_id << "] " << msg << std::endl;
}

/**
 * @brief Worker function: creates its own connection, inserts data, reads it
 *        back, and disconnects.
 */
static void worker(int thread_id, const std::string& connection_string)
{
	// Each thread creates its own context and manager
	auto context = std::make_shared<database_context>();
	auto db = std::make_shared<database_manager>(context);
	db->set_mode(database_types::postgres);

	auto connect_result = db->connect_result(connection_string);
	if (!connect_result.is_ok())
	{
		thread_print(thread_id, "Connection failed");
		return;
	}
	thread_print(thread_id, "Connected");

	// Insert a row identifying this thread
	std::string insert_sql
		= "INSERT INTO pool_demo (thread_id, message) VALUES ("
		  + std::to_string(thread_id) + ", 'Hello from thread "
		  + std::to_string(thread_id) + "')";

	auto insert_result = db->execute_query_result(insert_sql);
	if (insert_result.is_ok())
	{
		thread_print(thread_id, "Row inserted");
	}
	else
	{
		thread_print(thread_id, "Insert failed");
	}

	// Read back all rows (each thread sees the current state)
	auto select_result = db->select_query_result(
		"SELECT thread_id, message FROM pool_demo ORDER BY thread_id");
	if (select_result.is_ok())
	{
		thread_print(thread_id,
					 "Read " + std::to_string(select_result.value().size()) + " row(s)");
	}

	db->disconnect_result();
	thread_print(thread_id, "Disconnected");
}

int main()
{
	std::cout << "=== connection_pool_demo example ===" << std::endl;

	std::string connection_string
		= "host=localhost port=5432 dbname=example_db user=user password=password";

	// Setup: create the demo table using a single connection
	{
		auto context = std::make_shared<database_context>();
		auto db = std::make_shared<database_manager>(context);
		db->set_mode(database_types::postgres);

		auto connect_result = db->connect_result(connection_string);
		if (!connect_result.is_ok())
		{
			std::cerr << "Setup connection failed. Ensure PostgreSQL is running."
					  << std::endl;
			return 1;
		}

		db->create_query_result(R"(
			CREATE TABLE IF NOT EXISTS pool_demo (
				id        SERIAL PRIMARY KEY,
				thread_id INTEGER NOT NULL,
				message   VARCHAR(200)
			)
		)");
		db->execute_query_result("DELETE FROM pool_demo");
		db->disconnect_result();
		std::cout << "Setup complete" << std::endl;
	}

	// Launch multiple threads, each with its own connection
	const int num_threads = 4;
	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	std::cout << "\nLaunching " << num_threads << " worker threads..." << std::endl;

	for (int i = 0; i < num_threads; ++i)
	{
		threads.emplace_back(worker, i, connection_string);
	}

	// Wait for all threads to complete
	for (auto& t : threads)
	{
		t.join();
	}
	std::cout << "\nAll threads completed" << std::endl;

	// Final verification: read all inserted rows
	{
		auto context = std::make_shared<database_context>();
		auto db = std::make_shared<database_manager>(context);
		db->set_mode(database_types::postgres);

		auto connect_result = db->connect_result(connection_string);
		if (connect_result.is_ok())
		{
			auto result = db->select_query_result(
				"SELECT thread_id, message FROM pool_demo ORDER BY thread_id");
			if (result.is_ok())
			{
				std::cout << "\nFinal table contents (" << result.value().size()
						  << " rows):" << std::endl;
				for (const auto& row : result.value())
				{
					for (const auto& [col, val] : row)
					{
						std::cout << "  " << col << " = ";
						std::visit([](const auto& v) { std::cout << v; }, val);
						std::cout << "  ";
					}
					std::cout << std::endl;
				}
			}
			db->disconnect_result();
		}
	}

	std::cout << "\n=== connection_pool_demo example completed ===" << std::endl;
	return 0;
}
