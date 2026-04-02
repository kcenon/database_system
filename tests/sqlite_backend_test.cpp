// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * SQLite Backend Tests (DB-001)
 *
 * Tests for SQLite backend implementation covering:
 * - Connection handling (file database, memory database, invalid paths)
 * - CRUD operations (INSERT, SELECT, UPDATE, DELETE)
 * - Error handling and edge cases
 * - Thread safety with recursive_mutex
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

#include "database/backends/sqlite_backend.h"
#include "database/core/database_backend.h"

using namespace database;
using namespace database::backends;
using namespace database::core;

/**
 * @class SQLiteBackendTest
 * @brief Test fixture for SQLite backend tests
 */
class SQLiteBackendTest : public ::testing::Test {
protected:
  std::unique_ptr<sqlite_backend> backend_;
  std::string test_db_path_;

  void SetUp() override {
    backend_ = std::make_unique<sqlite_backend>();
    test_db_path_ = "test_sqlite_" + std::to_string(std::time(nullptr)) + ".db";
  }

  void TearDown() override {
    if (backend_ && backend_->is_initialized()) {
      backend_->shutdown();
    }
    if (std::filesystem::exists(test_db_path_)) {
      std::filesystem::remove(test_db_path_);
    }
  }

  bool connectToMemory() {
    connection_config config;
    config.database = ":memory:";
    return backend_->initialize(config).is_ok();
  }

  bool connectToFile() {
    connection_config config;
    config.database = test_db_path_;
    return backend_->initialize(config).is_ok();
  }

  bool createTestTable() {
    return backend_->execute_query("CREATE TABLE IF NOT EXISTS test_table ("
                                     "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                     "  name TEXT NOT NULL,"
                                     "  value REAL,"
                                     "  active INTEGER DEFAULT 1"
                                     ")").is_ok();
  }
};

//=============================================================================
// Database Type Tests
//=============================================================================

TEST_F(SQLiteBackendTest, DatabaseTypeReturnsSQLite) {
  EXPECT_EQ(backend_->type(), database_types::sqlite);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(SQLiteBackendTest, ConnectToMemoryDatabase) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(backend_->is_initialized());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

TEST_F(SQLiteBackendTest, ConnectToFileDatabase) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToFile());
  EXPECT_TRUE(std::filesystem::exists(test_db_path_));
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

TEST_F(SQLiteBackendTest, ShutdownWithoutConnection) {
  auto result = backend_->shutdown();
  EXPECT_TRUE(result.is_ok());
}

TEST_F(SQLiteBackendTest, ShutdownAfterConnection) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(backend_->shutdown().is_ok());
  EXPECT_FALSE(backend_->is_initialized());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// CREATE Query Tests
//=============================================================================

TEST_F(SQLiteBackendTest, ExecuteQueryWithoutConnection) {
  EXPECT_FALSE(backend_->execute_query("CREATE TABLE test (id INTEGER)").is_ok());
}

TEST_F(SQLiteBackendTest, CreateTableQuery) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(backend_->execute_query(
      "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)").is_ok());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// INSERT Query Tests
//=============================================================================

TEST_F(SQLiteBackendTest, InsertQueryWithoutConnection) {
  auto result = backend_->execute_query("INSERT INTO test VALUES (1, 'test')");
  EXPECT_FALSE(result.is_ok());
}

TEST_F(SQLiteBackendTest, InsertSingleRow) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  auto result = backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('item1', 10.5)");
  EXPECT_TRUE(result.is_ok());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// SELECT Query Tests
//=============================================================================

TEST_F(SQLiteBackendTest, SelectQueryWithoutConnection) {
  auto result = backend_->select_query("SELECT * FROM test");
  EXPECT_FALSE(result.is_ok());
}

TEST_F(SQLiteBackendTest, SelectFromEmptyTable) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  auto result = backend_->select_query("SELECT * FROM test_table");
  EXPECT_TRUE(result.is_ok());
  EXPECT_TRUE(result.value().empty());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

TEST_F(SQLiteBackendTest, SelectWithCondition) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('item1', 10.0)");
  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('item2', 20.0)");
  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('item3', 30.0)");

  auto result =
      backend_->select_query("SELECT * FROM test_table WHERE value > 15.0");
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value().size(), 2u);
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// UPDATE Query Tests
//=============================================================================

TEST_F(SQLiteBackendTest, UpdateQueryWithoutConnection) {
  auto result = backend_->execute_query("UPDATE test SET name = 'new'");
  EXPECT_FALSE(result.is_ok());
}

TEST_F(SQLiteBackendTest, UpdateSingleRow) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('old_name', 10.0)");

  auto result = backend_->execute_query(
      "UPDATE test_table SET name = 'new_name' WHERE id = 1");
  EXPECT_TRUE(result.is_ok());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// DELETE Query Tests
//=============================================================================

TEST_F(SQLiteBackendTest, DeleteQueryWithoutConnection) {
  auto result = backend_->execute_query("DELETE FROM test WHERE id = 1");
  EXPECT_FALSE(result.is_ok());
}

TEST_F(SQLiteBackendTest, DeleteSingleRow) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('item1', 10.0)");
  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('item2', 20.0)");

  auto result =
      backend_->execute_query("DELETE FROM test_table WHERE id = 1");
  EXPECT_TRUE(result.is_ok());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// Transaction Tests
//=============================================================================

TEST_F(SQLiteBackendTest, TransactionCommit) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  EXPECT_TRUE(backend_->begin_transaction().is_ok());
  EXPECT_TRUE(backend_->in_transaction());
  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('trans_item', 50.0)");
  EXPECT_TRUE(backend_->commit_transaction().is_ok());
  EXPECT_FALSE(backend_->in_transaction());

  auto result = backend_->select_query(
      "SELECT * FROM test_table WHERE name = 'trans_item'");
  EXPECT_TRUE(result.is_ok());
  EXPECT_EQ(result.value().size(), 1u);
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

TEST_F(SQLiteBackendTest, TransactionRollback) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  EXPECT_TRUE(backend_->begin_transaction().is_ok());
  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('rollback_item', 50.0)");
  EXPECT_TRUE(backend_->rollback_transaction().is_ok());

  auto result = backend_->select_query(
      "SELECT * FROM test_table WHERE name = 'rollback_item'");
  EXPECT_TRUE(result.is_ok());
  EXPECT_TRUE(result.value().empty());
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(SQLiteBackendTest, ConcurrentReads) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  for (int i = 0; i < 100; ++i) {
    backend_->execute_query(
        "INSERT INTO test_table (name, value) VALUES ('item" +
        std::to_string(i) + "', " + std::to_string(i * 1.5) + ")");
  }

  std::atomic<int> success_count{0};
  std::vector<std::thread> threads;

  for (int t = 0; t < 10; ++t) {
    threads.emplace_back([this, &success_count]() {
      for (int i = 0; i < 10; ++i) {
        auto result =
            backend_->select_query("SELECT COUNT(*) as cnt FROM test_table");
        if (result.is_ok() && std::get<int64_t>(result.value()[0].at("cnt")) == 100) {
          success_count++;
        }
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(success_count.load(), 100);
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SQLiteBackendTest, SpecialCharactersInData) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('test''s data', 10.0)");

  auto result = backend_->select_query("SELECT name FROM test_table");
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(std::get<std::string>(result.value()[0].at("name")), "test's data");
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

TEST_F(SQLiteBackendTest, UnicodeData) {
#ifdef USE_SQLITE
  EXPECT_TRUE(connectToMemory());
  EXPECT_TRUE(createTestTable());

  backend_->execute_query(
      "INSERT INTO test_table (name, value) VALUES ('한글테스트', 10.0)");

  auto result = backend_->select_query("SELECT name FROM test_table");
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(std::get<std::string>(result.value()[0].at("name")), "한글테스트");
#else
  GTEST_SKIP() << "SQLite support not compiled";
#endif
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
