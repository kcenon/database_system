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

#include "database/core/database_context.h"
#include "database/database_manager.h"
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

namespace database::testing {
/**
 * @class DatabaseSystemFixture
 * @brief Base test fixture for database system integration tests.
 *
 * Provides common setup and teardown functionality for database tests,
 * including test database initialization and cleanup.
 */
class DatabaseSystemFixture : public ::testing::Test {
protected:
  void SetUp() override {
#ifndef USE_SQLITE
    GTEST_SKIP()
        << "SQLite support not compiled. "
        << "Build with --with-sqlite or -DUSE_SQLITE=ON to enable these tests.";
#endif

    // Create unique test database file
    test_db_path_ =
        std::filesystem::temp_directory_path() /
        ("test_db_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".db");

    // Initialize database manager with dependency injection
    context_ = std::make_shared<database_context>();
    manager_ptr_ = std::make_shared<database_manager>(context_);
    manager_ = manager_ptr_.get();
    manager_->set_mode(database_types::sqlite);

    // Connect to test database - use absolute path without URI prefix
    auto connect_result = manager_->connect_result(test_db_path_.string());
    connected_ = connect_result.is_ok();

    if (connected_) {
      // Create test tables
      CreateTestTables();
    } else {
      std::cerr << "Failed to connect to test database: " << test_db_path_
                << std::endl;
      GTEST_SKIP() << "Failed to connect to SQLite test database";
    }
  }

  void TearDown() override {
    // Disconnect from database
    if (connected_) {
      manager_->disconnect_result();
      connected_ = false;
    }

    // Yield to allow cleanup to complete
    std::this_thread::yield();

    // Clean up test database file
    if (std::filesystem::exists(test_db_path_)) {
      std::error_code ec;
      std::filesystem::remove(test_db_path_, ec);
      // Retry if file is still locked
      if (std::filesystem::exists(test_db_path_)) {
        std::this_thread::yield();
        std::filesystem::remove(test_db_path_, ec);
      }
    }
  }

  /**
   * @brief Creates standard test tables.
   */
  virtual void CreateTestTables() {
    manager_->create_query_result("CREATE TABLE IF NOT EXISTS users ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "name TEXT NOT NULL, "
                           "email TEXT UNIQUE NOT NULL, "
                           "age INTEGER, "
                           "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
                           ")");

    manager_->create_query_result("CREATE TABLE IF NOT EXISTS products ("
                           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "name TEXT NOT NULL, "
                           "price REAL NOT NULL, "
                           "stock INTEGER DEFAULT 0"
                           ")");
  }

  /**
   * @brief Executes a query and returns result.
   * @param query SQL query to execute
   * @return Query result
   */
  core::database_result ExecuteQuery(const std::string &query) {
    auto result = manager_->select_query_result(query);
    if (result.is_ok()) {
      return result.value();
    }
    return {};
  }

  /**
   * @brief Creates a test table with custom schema.
   * @param table_name Name of the table
   * @param schema Table schema SQL
   * @return true if successful
   */
  bool CreateTestTable(const std::string &table_name,
                       const std::string &schema) {
    std::string query =
        "CREATE TABLE IF NOT EXISTS " + table_name + " (" + schema + ")";
    return manager_->create_query_result(query).is_ok();
  }

  /**
   * @brief Drops a test table.
   * @param table_name Name of the table to drop
   * @return true if successful
   */
  bool DropTestTable(const std::string &table_name) {
    std::string query = "DROP TABLE IF EXISTS " + table_name;
    return manager_->create_query_result(query).is_ok();
  }

  /**
   * @brief Inserts test data into users table.
   * @param count Number of users to insert
   * @return Number of rows inserted
   */
  size_t InsertTestUsers(size_t count) {
    size_t inserted = 0;
    for (size_t i = 0; i < count; ++i) {
      std::string query = "INSERT INTO users (name, email, age) VALUES ("
                          "'User" +
                          std::to_string(i) +
                          "', "
                          "'user" +
                          std::to_string(i) + "@test.com', " +
                          std::to_string(20 + (i % 50)) + ")";
      auto result = manager_->execute_query_result(query);
      if (result.is_ok()) {
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
  bool VerifyRowCount(const std::string &table_name, size_t expected_count) {
    auto result = ExecuteQuery("SELECT COUNT(*) as cnt FROM " + table_name);
    if (result.empty()) {
      return false;
    }

    auto it = result[0].find("cnt");
    if (it == result[0].end()) {
      return false;
    }

    // Extract value from variant
    try {
      // Try int64_t first (most common for COUNT)
      if (std::holds_alternative<int64_t>(it->second)) {
        return static_cast<size_t>(std::get<int64_t>(it->second)) ==
               expected_count;
      }
      // Try string (some databases return count as string)
      if (std::holds_alternative<std::string>(it->second)) {
        return std::stoul(std::get<std::string>(it->second)) == expected_count;
      }
      // Try double
      if (std::holds_alternative<double>(it->second)) {
        return static_cast<size_t>(std::get<double>(it->second)) ==
               expected_count;
      }
    } catch (...) {
      return false;
    }
    return false;
  }

  /**
   * @brief Clears all data from a table.
   * @param table_name Table name
   */
  void ClearTable(const std::string &table_name) {
    manager_->execute_query_result("DELETE FROM " + table_name);
  }

protected:
  std::shared_ptr<database_context> context_;
  std::shared_ptr<database_manager> manager_ptr_;
  database_manager *manager_{nullptr}; // Raw pointer for backward compatibility
  std::filesystem::path test_db_path_;
  bool connected_{false};
};

} // namespace database::testing
