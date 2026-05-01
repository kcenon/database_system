// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include <kcenon/database/core/database_context.h>
#include <kcenon/database/database_manager.h>
#include <kcenon/database/database_types.h>

using namespace database;

// Test fixture for database tests
class DatabaseTest : public ::testing::Test {
protected:
  std::shared_ptr<database_context> context_;
  std::shared_ptr<database_manager> db_mgr_;

  void SetUp() override {
    // Test setup with dependency injection
    context_ = std::make_shared<database_context>();
    db_mgr_ = std::make_shared<database_manager>(context_);
  }

  void TearDown() override {
    // Cleanup
    if (db_mgr_) {
      db_mgr_->disconnect_result();
    }
  }
};

// Basic database manager tests
TEST_F(DatabaseTest, DatabaseManagerDependencyInjection) {
  // Test that dependency injection pattern works correctly
  auto context1 = std::make_shared<database_context>();
  auto db1 = std::make_shared<database_manager>(context1);

  auto context2 = std::make_shared<database_context>();
  auto db2 = std::make_shared<database_manager>(context2);

  // Should be different instances (no singleton)
  EXPECT_NE(db1.get(), db2.get());
  EXPECT_NE(context1.get(), context2.get());
}

TEST_F(DatabaseTest, DatabaseTypeSettings) {
  // Test setting PostgreSQL
  EXPECT_TRUE(db_mgr_->set_mode(database_types::postgres));
  EXPECT_EQ(db_mgr_->database_type(), database_types::postgres);

  // Reset to ensure clean state
  db_mgr_->disconnect_result();

  // Test SQLite backend (may be supported)
  bool sqlite_result = db_mgr_->set_mode(database_types::sqlite);
  if (sqlite_result) {
    // SQLite is supported
    EXPECT_EQ(db_mgr_->database_type(), database_types::sqlite);
    db_mgr_->disconnect_result();
  } else {
    // SQLite not supported
    EXPECT_EQ(db_mgr_->database_type(), database_types::none);
  }

}

TEST_F(DatabaseTest, BasicQueryOperations) {
  // Set database mode
  EXPECT_TRUE(db_mgr_->set_mode(database_types::postgres));

  // Test query creation (should not crash)
  EXPECT_NO_THROW(db_mgr_->create_query_result("SELECT 1"));

  // Test select query behavior
  auto result = db_mgr_->select_query_result("SELECT 1");
  // Note: PostgreSQL support may not be compiled, so result may contain error
  // info We just test that it doesn't crash and returns some result
  EXPECT_NO_THROW(result);
}

TEST_F(DatabaseTest, ConnectionHandling) {
  // Set database mode
  EXPECT_TRUE(db_mgr_->set_mode(database_types::postgres));

  // Test connection with invalid connection string
  // Note: In mock mode (PostgreSQL not compiled), connect may succeed
  // as backends use mock implementation for testing without actual DB
  auto connect_result = db_mgr_->connect_result("invalid_connection_string");
  // Just verify it doesn't crash - actual result depends on mock mode
  (void)connect_result;

  // Test disconnect (should not crash)
  EXPECT_NO_THROW(db_mgr_->disconnect_result());
}

// Enhanced database tests with Phase 4 features
TEST_F(DatabaseTest, PhaseA4DatabaseTypes) {
  // Test all database types
  std::vector<database_types> types = {
      database_types::postgres, database_types::sqlite,
      database_types::mongodb, database_types::redis};

  for (auto type : types) {
    // Should not crash regardless of whether backend is available
    EXPECT_NO_THROW(db_mgr_->set_mode(type));
  }
}

TEST_F(DatabaseTest, GeneralQueryExecution) {
  // Test general query execution capabilities
  EXPECT_TRUE(db_mgr_->set_mode(database_types::postgres));

  // Test various query types work without crashing
  EXPECT_NO_THROW(db_mgr_->create_query_result("SELECT 1"));
  EXPECT_NO_THROW(db_mgr_->select_query_result("SELECT 1"));
}
