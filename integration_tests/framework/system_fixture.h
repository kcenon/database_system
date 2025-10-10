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

#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <filesystem>
#include "database/database_manager.h"
#include "database/connection_pool.h"
#include "database/backends/sqlite/sqlite_manager.h"

namespace database::testing
{
	/**
	 * @class DatabaseSystemFixture
	 * @brief Base test fixture for database system integration tests.
	 *
	 * Provides common setup and teardown functionality for database tests,
	 * including test database initialization and cleanup.
	 */
	class DatabaseSystemFixture : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			// Create unique test database file
			test_db_path_ = std::filesystem::temp_directory_path() /
			                ("test_db_" + std::to_string(
			                    std::chrono::steady_clock::now().time_since_epoch().count()) +
			                 ".db");

			// Initialize database manager
			manager_ = &database_manager::handle();
			manager_->set_mode(database_types::sqlite);

			// Connect to test database
			std::string connection_string = "file:" + test_db_path_.string();
			connected_ = manager_->connect(connection_string);

			if (connected_) {
				// Create test tables
				CreateTestTables();
			}
		}

		void TearDown() override
		{
			// Disconnect from database
			if (connected_) {
				manager_->disconnect();
			}

			// Shutdown all connection pools
			connection_pool_manager::instance().shutdown_all();

			// Clean up test database file
			if (std::filesystem::exists(test_db_path_)) {
				std::error_code ec;
				std::filesystem::remove(test_db_path_, ec);
			}
		}

		/**
		 * @brief Creates standard test tables.
		 */
		virtual void CreateTestTables()
		{
			manager_->create_query(
				"CREATE TABLE IF NOT EXISTS users ("
				"id INTEGER PRIMARY KEY AUTOINCREMENT, "
				"name TEXT NOT NULL, "
				"email TEXT UNIQUE NOT NULL, "
				"age INTEGER, "
				"created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
				")"
			);

			manager_->create_query(
				"CREATE TABLE IF NOT EXISTS products ("
				"id INTEGER PRIMARY KEY AUTOINCREMENT, "
				"name TEXT NOT NULL, "
				"price REAL NOT NULL, "
				"stock INTEGER DEFAULT 0"
				")"
			);
		}

		/**
		 * @brief Gets a connection from the pool.
		 * @return Shared pointer to connection wrapper
		 */
		std::shared_ptr<connection_wrapper> GetConnection()
		{
			auto pool = connection_pool_manager::instance().get_pool(database_types::sqlite);
			if (!pool) {
				return nullptr;
			}
			return pool->acquire_connection();
		}

		/**
		 * @brief Executes a query and returns result.
		 * @param query SQL query to execute
		 * @return Query result
		 */
		database_result ExecuteQuery(const std::string& query)
		{
			return manager_->select_query(query);
		}

		/**
		 * @brief Creates a test table with custom schema.
		 * @param table_name Name of the table
		 * @param schema Table schema SQL
		 * @return true if successful
		 */
		bool CreateTestTable(const std::string& table_name, const std::string& schema)
		{
			std::string query = "CREATE TABLE IF NOT EXISTS " + table_name + " (" + schema + ")";
			return manager_->create_query(query);
		}

		/**
		 * @brief Drops a test table.
		 * @param table_name Name of the table to drop
		 * @return true if successful
		 */
		bool DropTestTable(const std::string& table_name)
		{
			std::string query = "DROP TABLE IF EXISTS " + table_name;
			return manager_->create_query(query);
		}

		/**
		 * @brief Inserts test data into users table.
		 * @param count Number of users to insert
		 * @return Number of rows inserted
		 */
		size_t InsertTestUsers(size_t count)
		{
			size_t inserted = 0;
			for (size_t i = 0; i < count; ++i) {
				std::string query = "INSERT INTO users (name, email, age) VALUES ("
				                   "'User" + std::to_string(i) + "', "
				                   "'user" + std::to_string(i) + "@test.com', "
				                   + std::to_string(20 + (i % 50)) + ")";
				if (manager_->insert_query(query) > 0) {
					++inserted;
				}
			}
			return inserted;
		}

		/**
		 * @brief Verifies row count in a table.
		 * @param table_name Table name
		 * @param expected_count Expected row count
		 * @return true if count matches
		 */
		bool VerifyRowCount(const std::string& table_name, size_t expected_count)
		{
			auto result = ExecuteQuery("SELECT COUNT(*) as cnt FROM " + table_name);
			if (result.empty()) {
				return false;
			}

			auto it = result[0].find("cnt");
			if (it == result[0].end()) {
				return false;
			}

			return std::stoul(it->second) == expected_count;
		}

		/**
		 * @brief Clears all data from a table.
		 * @param table_name Table name
		 */
		void ClearTable(const std::string& table_name)
		{
			manager_->delete_query("DELETE FROM " + table_name);
		}

	protected:
		database_manager* manager_{nullptr};
		std::filesystem::path test_db_path_;
		bool connected_{false};
	};

	/**
	 * @class ConnectionPoolFixture
	 * @brief Test fixture with connection pool support.
	 */
	class ConnectionPoolFixture : public DatabaseSystemFixture
	{
	protected:
		void SetUp() override
		{
			DatabaseSystemFixture::SetUp();

			// Create connection pool
			if (connected_) {
				connection_pool_config config;
				config.min_connections = 2;
				config.max_connections = 10;
				config.acquire_timeout = std::chrono::milliseconds(5000);
				config.idle_timeout = std::chrono::milliseconds(30000);
				config.health_check_interval = std::chrono::milliseconds(60000);
				config.enable_health_checks = true;
				config.connection_string = "file:" + test_db_path_.string();

				pool_created_ = connection_pool_manager::instance().create_pool(
					database_types::sqlite, config);
			}
		}

	protected:
		bool pool_created_{false};
	};

} // namespace database::testing
