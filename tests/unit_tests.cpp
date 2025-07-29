/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2021, 🍀☀🌕🌥 🌊
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

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "../database_manager.h"
#include "../postgres_manager.h"
#include "../database_types.h"
#include <container.h>

using namespace database;
using namespace container_module;

// Test fixture for database tests
class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset singleton state if needed
    }
    
    void TearDown() override {
        // Ensure cleanup
        auto& db = database_manager::handle();
        db.disconnect();
    }
    
    // Helper to check if PostgreSQL is available
    bool IsPostgreSQLAvailable() {
        auto& db = database_manager::handle();
        db.set_mode(database_types::postgres);
        
        // Try to connect to default postgres database
        // This assumes PostgreSQL is running locally with default settings
        return db.connect("host=localhost port=5432 dbname=postgres user=postgres");
    }
    
    // Helper to create test table
    bool CreateTestTable() {
        auto& db = database_manager::handle();
        
        // Drop table if exists
        db.create_query("DROP TABLE IF EXISTS test_table");
        
        // Create test table
        return db.create_query(
            "CREATE TABLE test_table ("
            "id SERIAL PRIMARY KEY,"
            "name VARCHAR(255) NOT NULL,"
            "age INTEGER,"
            "active BOOLEAN DEFAULT true,"
            "score DOUBLE PRECISION,"
            "data BYTEA,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        );
    }
};

// Database Types Tests
TEST(DatabaseTypesTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(database_types::none), 0);
    EXPECT_EQ(static_cast<int>(database_types::postgres), 1);
}

// Database Manager Singleton Tests
TEST(DatabaseManagerTest, SingletonInstance) {
    auto& instance1 = database_manager::handle();
    auto& instance2 = database_manager::handle();
    
    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);
}

TEST(DatabaseManagerTest, DefaultState) {
    auto& db = database_manager::handle();
    
    // Default database type should be none
    EXPECT_EQ(db.database_type(), database_types::none);
}

TEST(DatabaseManagerTest, SetMode) {
    auto& db = database_manager::handle();
    
    // Test setting PostgreSQL mode
    EXPECT_TRUE(db.set_mode(database_types::postgres));
    EXPECT_EQ(db.database_type(), database_types::postgres);
    
    // Currently only PostgreSQL is supported
}

// Connection Tests (requires PostgreSQL)
TEST_F(DatabaseTest, ConnectDisconnect) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    
    // Should be connected from IsPostgreSQLAvailable()
    EXPECT_TRUE(db.disconnect());
    
    // Second disconnect should fail
    EXPECT_FALSE(db.disconnect());
}

TEST_F(DatabaseTest, InvalidConnection) {
    auto& db = database_manager::handle();
    db.set_mode(database_types::postgres);
    
    // Invalid connection string
    EXPECT_FALSE(db.connect("invalid_connection_string"));
    
    // Invalid host
    EXPECT_FALSE(db.connect("host=nonexistent_host port=5432 dbname=test"));
}

// Query Tests (requires PostgreSQL)
TEST_F(DatabaseTest, CreateQuery) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    
    // Create table
    EXPECT_TRUE(CreateTestTable());
    
    // Create index
    EXPECT_TRUE(db.create_query("CREATE INDEX idx_test_name ON test_table(name)"));
    
    // Invalid query
    EXPECT_FALSE(db.create_query("INVALID SQL SYNTAX"));
}

TEST_F(DatabaseTest, InsertQuery) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    ASSERT_TRUE(CreateTestTable());
    
    // Insert single row
    unsigned int rows = db.insert_query(
        "INSERT INTO test_table (name, age, score) "
        "VALUES ('John Doe', 30, 85.5)"
    );
    EXPECT_EQ(rows, 1);
    
    // Insert multiple rows
    rows = db.insert_query(
        "INSERT INTO test_table (name, age, score) "
        "VALUES ('Jane Smith', 25, 92.0), ('Bob Johnson', 35, 78.5)"
    );
    EXPECT_EQ(rows, 2);
    
    // Insert with NULL values
    rows = db.insert_query(
        "INSERT INTO test_table (name) VALUES ('No Age')"
    );
    EXPECT_EQ(rows, 1);
}

TEST_F(DatabaseTest, UpdateQuery) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    ASSERT_TRUE(CreateTestTable());
    
    // Insert test data
    db.insert_query(
        "INSERT INTO test_table (name, age, active) "
        "VALUES ('Update Test', 20, true), ('Another User', 25, true)"
    );
    
    // Update single row
    unsigned int rows = db.update_query(
        "UPDATE test_table SET age = 21 WHERE name = 'Update Test'"
    );
    EXPECT_EQ(rows, 1);
    
    // Update multiple rows
    rows = db.update_query(
        "UPDATE test_table SET active = false WHERE age < 30"
    );
    EXPECT_EQ(rows, 2);
    
    // Update with no matches
    rows = db.update_query(
        "UPDATE test_table SET age = 100 WHERE name = 'Nonexistent'"
    );
    EXPECT_EQ(rows, 0);
}

TEST_F(DatabaseTest, DeleteQuery) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    ASSERT_TRUE(CreateTestTable());
    
    // Insert test data
    db.insert_query(
        "INSERT INTO test_table (name, age) "
        "VALUES ('Delete Me', 30), ('Keep Me', 25), ('Delete Me Too', 35)"
    );
    
    // Delete specific rows
    unsigned int rows = db.delete_query(
        "DELETE FROM test_table WHERE age > 30"
    );
    EXPECT_EQ(rows, 1);
    
    // Delete with pattern
    rows = db.delete_query(
        "DELETE FROM test_table WHERE name LIKE 'Delete%'"
    );
    EXPECT_EQ(rows, 1);
    
    // Delete all remaining
    rows = db.delete_query("DELETE FROM test_table");
    EXPECT_EQ(rows, 1);
}

TEST_F(DatabaseTest, SelectQuery) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    ASSERT_TRUE(CreateTestTable());
    
    // Insert test data
    db.insert_query(
        "INSERT INTO test_table (name, age, score, active) VALUES "
        "('Alice', 25, 90.5, true), "
        "('Bob', 30, 85.0, false), "
        "('Charlie', NULL, 95.5, true)"
    );
    
    // Select all
    auto result = db.select_query("SELECT * FROM test_table ORDER BY name");
    ASSERT_NE(result, nullptr);
    
    auto rows = result->value_array("row");
    ASSERT_EQ(rows.size(), 3);
    
    // Check first row (Alice)
    auto alice = rows[0];
    ASSERT_NE(alice, nullptr);
    // Rows are stored as serialized container values
    ASSERT_TRUE(alice->is_container());
    auto alice_container = std::make_unique<value_container>(alice->data());
    EXPECT_EQ(alice_container->get_value("name")->to_string(), "Alice");
    EXPECT_EQ(alice_container->get_value("age")->to_int(), 25);
    EXPECT_DOUBLE_EQ(alice_container->get_value("score")->to_double(), 90.5);
    EXPECT_TRUE(alice_container->get_value("active")->to_boolean());
    
    // Check NULL handling (Charlie)
    auto charlie = rows[2];
    ASSERT_NE(charlie, nullptr);
    ASSERT_TRUE(charlie->is_container());
    auto charlie_container = std::make_unique<value_container>(charlie->data());
    EXPECT_EQ(charlie_container->get_value("name")->to_string(), "Charlie");
    EXPECT_EQ(charlie_container->get_value("age")->type(), value_types::null_value);
    
    // Select with WHERE clause
    result = db.select_query("SELECT name, age FROM test_table WHERE active = true");
    ASSERT_NE(result, nullptr);
    rows = result->value_array("row");
    EXPECT_EQ(rows.size(), 2);
    
    // Empty result set
    result = db.select_query("SELECT * FROM test_table WHERE age > 100");
    ASSERT_NE(result, nullptr);
    rows = result->value_array("row");
    EXPECT_EQ(rows.size(), 0);
}

// Data Type Tests
TEST_F(DatabaseTest, DataTypes) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    ASSERT_TRUE(CreateTestTable());
    
    // Test various data types
    std::string binary_data = "Binary\x00Data\x01Test";
    
    // Note: PostgreSQL requires proper escaping for binary data
    // This is a simplified test - real implementation should use parameterized queries
    db.insert_query(
        "INSERT INTO test_table (name, age, score, active, data) VALUES "
        "('Type Test', 42, 3.14159, false, E'\\\\x42696E617279')"
    );
    
    auto result = db.select_query("SELECT * FROM test_table WHERE name = 'Type Test'");
    ASSERT_NE(result, nullptr);
    
    auto rows = result->value_array("row");
    ASSERT_EQ(rows.size(), 1);
    
    auto row = rows[0];
    ASSERT_TRUE(row->is_container());
    auto row_container = std::make_unique<value_container>(row->data());
    EXPECT_EQ(row_container->get_value("age")->to_int(), 42);
    EXPECT_NEAR(row_container->get_value("score")->to_double(), 3.14159, 0.00001);
    EXPECT_FALSE(row_container->get_value("active")->to_boolean());
}

// Thread Safety Tests
TEST_F(DatabaseTest, ThreadSafeSingleton) {
    const int thread_count = 10;
    std::vector<std::thread> threads;
    std::vector<database_manager*> instances(thread_count);
    
    // Multiple threads accessing singleton
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&instances, i]() {
            instances[i] = &database_manager::handle();
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All should be the same instance
    for (int i = 1; i < thread_count; ++i) {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

// Error Handling Tests
TEST_F(DatabaseTest, QueryWithoutConnection) {
    auto& db = database_manager::handle();
    db.set_mode(database_types::postgres);
    
    // Ensure disconnected
    db.disconnect();
    
    // All queries should fail without connection
    EXPECT_FALSE(db.create_query("CREATE TABLE test (id INT)"));
    EXPECT_EQ(db.insert_query("INSERT INTO test VALUES (1)"), 0);
    EXPECT_EQ(db.update_query("UPDATE test SET id = 2"), 0);
    EXPECT_EQ(db.delete_query("DELETE FROM test"), 0);
    
    auto result = db.select_query("SELECT * FROM test");
    EXPECT_EQ(result, nullptr);
}

// Special Character Handling
TEST_F(DatabaseTest, SpecialCharacters) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    ASSERT_TRUE(CreateTestTable());
    
    // Test with quotes and special characters
    // Note: This test demonstrates the need for proper escaping
    // Real implementation should use parameterized queries
    std::string name_with_quote = "O'Brien";
    std::string escaped_name = "O''Brien";  // PostgreSQL escape syntax
    
    unsigned int rows = db.insert_query(
        "INSERT INTO test_table (name) VALUES ('" + escaped_name + "')"
    );
    EXPECT_EQ(rows, 1);
    
    auto result = db.select_query("SELECT * FROM test_table WHERE name = '" + escaped_name + "'");
    ASSERT_NE(result, nullptr);
    
    auto result_rows = result->value_array("row");
    ASSERT_EQ(result_rows.size(), 1);
    ASSERT_TRUE(result_rows[0]->is_container());
    auto result_row = std::make_unique<value_container>(result_rows[0]->data());
    EXPECT_EQ(result_row->get_value("name")->to_string(), name_with_quote);
}

// Transaction Tests (if supported)
TEST_F(DatabaseTest, TransactionSupport) {
    if (!IsPostgreSQLAvailable()) {
        GTEST_SKIP() << "PostgreSQL not available";
    }
    
    auto& db = database_manager::handle();
    ASSERT_TRUE(CreateTestTable());
    
    // Begin transaction
    EXPECT_TRUE(db.create_query("BEGIN"));
    
    // Insert data
    db.insert_query("INSERT INTO test_table (name) VALUES ('Transaction Test')");
    
    // Rollback
    EXPECT_TRUE(db.create_query("ROLLBACK"));
    
    // Data should not exist
    auto result = db.select_query("SELECT * FROM test_table WHERE name = 'Transaction Test'");
    ASSERT_NE(result, nullptr);
    auto rows = result->value_array("row");
    EXPECT_EQ(rows.size(), 0);
    
    // Test commit
    EXPECT_TRUE(db.create_query("BEGIN"));
    db.insert_query("INSERT INTO test_table (name) VALUES ('Commit Test')");
    EXPECT_TRUE(db.create_query("COMMIT"));
    
    // Data should exist
    result = db.select_query("SELECT * FROM test_table WHERE name = 'Commit Test'");
    ASSERT_NE(result, nullptr);
    rows = result->value_array("row");
    EXPECT_EQ(rows.size(), 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}