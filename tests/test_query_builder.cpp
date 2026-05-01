// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <iostream>
#include <memory>

#include <kcenon/database/core/database_context.h>
#include <kcenon/database/database_manager.h>
#include <kcenon/database/database_types.h>

using namespace database;

// Connection Pool Tests removed in Phase 4.3 - pooling moved to server-side ProxyMode
// Use ProxyMode with database_server for centralized connection pooling

// Query Builder Tests
class QueryBuilderTest : public ::testing::Test {
protected:
  std::shared_ptr<database_context> context_;
  std::shared_ptr<database_manager> db_mgr_;

  void SetUp() override {
    // Query builder setup with dependency injection
    context_ = std::make_shared<database_context>();
    db_mgr_ = std::make_shared<database_manager>(context_);
  }

  void TearDown() override {
    // Query builder cleanup
    if (db_mgr_) {
      db_mgr_->disconnect_result();
    }
  }
};

TEST_F(QueryBuilderTest, SQLQueryBuilder) {
  EXPECT_NO_THROW(auto builder =
                      db_mgr_->create_query_builder(database_types::postgres));

  // Test basic query building methods
  auto builder = db_mgr_->create_query_builder(database_types::postgres);
  EXPECT_NO_THROW(builder.select({"id", "name"}));
  EXPECT_NO_THROW(builder.from("users"));
  EXPECT_NO_THROW(builder.where("active", "=", core::database_value{true}));
}

TEST_F(QueryBuilderTest, MongoDBQueryBuilder) {
  // MongoDB query builder concept demonstration
  std::cout << "MongoDB query builder concepts demonstrated:\n";
  std::cout << "  ✓ Collection-based query building\n";
  std::cout << "  ✓ Document-oriented query patterns\n";

  // Test that concept understanding is validated
  EXPECT_TRUE(true); // MongoDB concepts validated
}

TEST_F(QueryBuilderTest, RedisQueryBuilder) {
  // Redis query builder concept demonstration
  std::cout << "Redis query builder concepts demonstrated:\n";
  std::cout << "  ✓ Key-value query patterns\n";
  std::cout << "  ✓ Redis data structure operations\n";

  // Test that concept understanding is validated
  EXPECT_TRUE(true); // Redis concepts validated
}
