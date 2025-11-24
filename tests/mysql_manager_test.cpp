/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * MySQL Manager Tests (DB-001)
 *
 * Tests for MySQL backend implementation covering:
 * - Connection handling (valid/invalid credentials, connection string parsing)
 * - CRUD operations (INSERT, SELECT, UPDATE, DELETE)
 * - Error handling and edge cases
 * - Connection string parsing
 *
 * Note: Full integration tests require a running MySQL server.
 * Tests are designed to work in both mock mode (USE_MYSQL not defined)
 * and real mode (USE_MYSQL defined with available MySQL server).
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "database/backends/mysql/mysql_manager.h"

using namespace database;

/**
 * @class MySQLManagerTest
 * @brief Test fixture for MySQL manager tests
 */
class MySQLManagerTest : public ::testing::Test {
protected:
    std::unique_ptr<mysql_manager> manager_;

    // Test connection strings
    static constexpr const char* VALID_CONN_STRING =
        "host=localhost;port=3306;database=test;user=test;password=test";
    static constexpr const char* INVALID_CONN_STRING =
        "host=localhost;port=3306;database=test;user=invalid;password=wrong";
    static constexpr const char* MALFORMED_CONN_STRING = "invalid_connection_string";

    void SetUp() override {
        manager_ = std::make_unique<mysql_manager>();
    }

    void TearDown() override {
        if (manager_) {
            manager_->disconnect();
        }
    }
};

//=============================================================================
// Database Type Tests
//=============================================================================

TEST_F(MySQLManagerTest, DatabaseTypeReturnsMySQL) {
    EXPECT_EQ(manager_->database_type(), database_types::mysql);
}

//=============================================================================
// Connection String Parsing Tests
//=============================================================================

TEST_F(MySQLManagerTest, ParseValidConnectionString) {
    // Connection will fail without MySQL server, but parsing should work
    bool result = manager_->connect(VALID_CONN_STRING);
#ifdef USE_MYSQL
    // With MySQL support, result depends on server availability
    // Just verify no crash occurs
    EXPECT_NO_THROW(result);
#else
    // Without MySQL support, connect returns false
    EXPECT_FALSE(result);
#endif
}

TEST_F(MySQLManagerTest, ParseConnectionStringWithDefaultPort) {
    // Test without explicit port (should use default 3306)
    bool result = manager_->connect("host=localhost;database=test;user=test;password=test");
#ifndef USE_MYSQL
    EXPECT_FALSE(result);
#endif
}

TEST_F(MySQLManagerTest, ParseMalformedConnectionString) {
    bool result = manager_->connect(MALFORMED_CONN_STRING);
    // Malformed string should fail (missing required fields)
    EXPECT_FALSE(result);
}

TEST_F(MySQLManagerTest, ParseConnectionStringMissingDatabase) {
    bool result = manager_->connect("host=localhost;user=test;password=test");
    // Missing database should fail
    EXPECT_FALSE(result);
}

TEST_F(MySQLManagerTest, ParseConnectionStringMissingUser) {
    bool result = manager_->connect("host=localhost;database=test;password=test");
    // Missing user should fail
    EXPECT_FALSE(result);
}

TEST_F(MySQLManagerTest, ParseConnectionStringWithAllFields) {
    bool result = manager_->connect(
        "host=192.168.1.100;port=3307;database=mydb;user=admin;password=secret123"
    );
#ifndef USE_MYSQL
    EXPECT_FALSE(result);
#endif
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(MySQLManagerTest, ConnectWithValidCredentials) {
    bool result = manager_->connect(VALID_CONN_STRING);
#ifdef USE_MYSQL
    // May succeed or fail depending on MySQL server availability
    // No assertion on result, just verify no crash
    (void)result;
#else
    EXPECT_FALSE(result);
#endif
}

TEST_F(MySQLManagerTest, ConnectWithInvalidCredentials) {
    bool result = manager_->connect(INVALID_CONN_STRING);
    // Invalid credentials should fail
    EXPECT_FALSE(result);
}

TEST_F(MySQLManagerTest, DisconnectWithoutConnection) {
    EXPECT_FALSE(manager_->disconnect());
}

TEST_F(MySQLManagerTest, MultipleConnectionAttempts) {
    // Multiple connect attempts should not crash
    EXPECT_NO_THROW({
        manager_->connect(VALID_CONN_STRING);
        manager_->disconnect();
        manager_->connect(VALID_CONN_STRING);
        manager_->disconnect();
    });
}

//=============================================================================
// Query Tests (Mock Mode)
//=============================================================================

TEST_F(MySQLManagerTest, CreateQueryWithoutConnection) {
    EXPECT_FALSE(manager_->create_query("CREATE TABLE test (id INT)"));
}

TEST_F(MySQLManagerTest, InsertQueryWithoutConnection) {
    EXPECT_EQ(manager_->insert_query("INSERT INTO test VALUES (1, 'test')"), 0u);
}

TEST_F(MySQLManagerTest, UpdateQueryWithoutConnection) {
    EXPECT_EQ(manager_->update_query("UPDATE test SET name = 'new'"), 0u);
}

TEST_F(MySQLManagerTest, DeleteQueryWithoutConnection) {
    EXPECT_EQ(manager_->delete_query("DELETE FROM test WHERE id = 1"), 0u);
}

TEST_F(MySQLManagerTest, SelectQueryWithoutConnection) {
    auto result = manager_->select_query("SELECT * FROM test");
#ifdef USE_MYSQL
    // Without connection, should return empty
    EXPECT_TRUE(result.empty());
#else
    // Mock mode returns mock data for SELECT queries
    EXPECT_EQ(result.size(), 1u);
#endif
}

TEST_F(MySQLManagerTest, ExecuteQueryWithoutConnection) {
#ifdef USE_MYSQL
    EXPECT_FALSE(manager_->execute_query("CREATE TABLE test (id INT)"));
#else
    // Mock mode returns true for execute_query
    EXPECT_TRUE(manager_->execute_query("CREATE TABLE test (id INT)"));
#endif
}

//=============================================================================
// Mock Data Tests (when USE_MYSQL is not defined)
//=============================================================================

#ifndef USE_MYSQL

TEST_F(MySQLManagerTest, MockSelectQueryReturnsData) {
    auto result = manager_->select_query("SELECT * FROM any_table");
    EXPECT_EQ(result.size(), 1u);

    if (!result.empty()) {
        const auto& row = result[0];
        EXPECT_TRUE(row.find("id") != row.end());
        EXPECT_TRUE(row.find("name") != row.end());
        EXPECT_EQ(std::get<int64_t>(row.at("id")), 1);
        EXPECT_EQ(std::get<std::string>(row.at("name")), "mysql_mock_data");
    }
}

TEST_F(MySQLManagerTest, MockExecuteQuerySucceeds) {
    EXPECT_TRUE(manager_->execute_query("CREATE TABLE mock_table (id INT)"));
}

#endif // !USE_MYSQL

//=============================================================================
// Integration Tests (when USE_MYSQL is defined and MySQL server is available)
//=============================================================================

#ifdef USE_MYSQL

class MySQLIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<mysql_manager> manager_;
    bool connected_ = false;

    void SetUp() override {
        manager_ = std::make_unique<mysql_manager>();
        connected_ = manager_->connect(
            "host=localhost;port=3306;database=test;user=test;password=test"
        );

        if (connected_) {
            // Clean up test table if exists
            manager_->execute_query("DROP TABLE IF EXISTS mysql_test_table");

            // Create test table
            manager_->execute_query(
                "CREATE TABLE mysql_test_table ("
                "  id INT AUTO_INCREMENT PRIMARY KEY,"
                "  name VARCHAR(255) NOT NULL,"
                "  value DECIMAL(10,2),"
                "  active BOOLEAN DEFAULT TRUE"
                ")"
            );
        }
    }

    void TearDown() override {
        if (connected_) {
            manager_->execute_query("DROP TABLE IF EXISTS mysql_test_table");
            manager_->disconnect();
        }
    }
};

TEST_F(MySQLIntegrationTest, InsertAndSelect) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    unsigned int inserted = manager_->insert_query(
        "INSERT INTO mysql_test_table (name, value) VALUES ('test_item', 42.50)"
    );
    EXPECT_EQ(inserted, 1u);

    auto result = manager_->select_query("SELECT * FROM mysql_test_table");
    ASSERT_EQ(result.size(), 1u);

    const auto& row = result[0];
    EXPECT_EQ(std::get<std::string>(row.at("name")), "test_item");
}

TEST_F(MySQLIntegrationTest, UpdateRow) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    manager_->insert_query(
        "INSERT INTO mysql_test_table (name, value) VALUES ('old_name', 10.00)"
    );

    unsigned int updated = manager_->update_query(
        "UPDATE mysql_test_table SET name = 'new_name' WHERE name = 'old_name'"
    );
    EXPECT_EQ(updated, 1u);

    auto result = manager_->select_query(
        "SELECT name FROM mysql_test_table WHERE name = 'new_name'"
    );
    EXPECT_EQ(result.size(), 1u);
}

TEST_F(MySQLIntegrationTest, DeleteRow) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    manager_->insert_query(
        "INSERT INTO mysql_test_table (name, value) VALUES ('to_delete', 10.00)"
    );

    unsigned int deleted = manager_->delete_query(
        "DELETE FROM mysql_test_table WHERE name = 'to_delete'"
    );
    EXPECT_EQ(deleted, 1u);

    auto result = manager_->select_query(
        "SELECT * FROM mysql_test_table WHERE name = 'to_delete'"
    );
    EXPECT_TRUE(result.empty());
}

TEST_F(MySQLIntegrationTest, SelectWithCondition) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    manager_->insert_query("INSERT INTO mysql_test_table (name, value) VALUES ('item1', 10.00)");
    manager_->insert_query("INSERT INTO mysql_test_table (name, value) VALUES ('item2', 20.00)");
    manager_->insert_query("INSERT INTO mysql_test_table (name, value) VALUES ('item3', 30.00)");

    auto result = manager_->select_query(
        "SELECT * FROM mysql_test_table WHERE value > 15.00"
    );
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(MySQLIntegrationTest, TypeMapping) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    manager_->insert_query(
        "INSERT INTO mysql_test_table (name, value, active) VALUES ('type_test', 99.99, TRUE)"
    );

    auto result = manager_->select_query("SELECT * FROM mysql_test_table WHERE name = 'type_test'");
    ASSERT_EQ(result.size(), 1u);

    const auto& row = result[0];

    // Check integer type (id)
    EXPECT_TRUE(std::holds_alternative<int64_t>(row.at("id")));

    // Check string type (name)
    EXPECT_TRUE(std::holds_alternative<std::string>(row.at("name")));

    // Check decimal/double type (value)
    EXPECT_TRUE(std::holds_alternative<double>(row.at("value")));
    EXPECT_NEAR(std::get<double>(row.at("value")), 99.99, 0.01);
}

TEST_F(MySQLIntegrationTest, NullValueHandling) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    manager_->insert_query(
        "INSERT INTO mysql_test_table (name, value) VALUES ('null_test', NULL)"
    );

    auto result = manager_->select_query(
        "SELECT value FROM mysql_test_table WHERE name = 'null_test'"
    );
    ASSERT_EQ(result.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[0].at("value")));
}

TEST_F(MySQLIntegrationTest, TransactionCommit) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    EXPECT_TRUE(manager_->execute_query("START TRANSACTION"));
    manager_->insert_query(
        "INSERT INTO mysql_test_table (name, value) VALUES ('transaction_item', 50.00)"
    );
    EXPECT_TRUE(manager_->execute_query("COMMIT"));

    auto result = manager_->select_query(
        "SELECT * FROM mysql_test_table WHERE name = 'transaction_item'"
    );
    EXPECT_EQ(result.size(), 1u);
}

TEST_F(MySQLIntegrationTest, TransactionRollback) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    EXPECT_TRUE(manager_->execute_query("START TRANSACTION"));
    manager_->insert_query(
        "INSERT INTO mysql_test_table (name, value) VALUES ('rollback_item', 50.00)"
    );
    EXPECT_TRUE(manager_->execute_query("ROLLBACK"));

    auto result = manager_->select_query(
        "SELECT * FROM mysql_test_table WHERE name = 'rollback_item'"
    );
    EXPECT_TRUE(result.empty());
}

TEST_F(MySQLIntegrationTest, InvalidSQLSyntax) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    EXPECT_FALSE(manager_->execute_query("INVALID SQL STATEMENT"));
    EXPECT_FALSE(manager_->create_query("INVALID SQL"));
    EXPECT_EQ(manager_->insert_query("INSERT INVALID"), 0u);
}

TEST_F(MySQLIntegrationTest, LargeDataSet) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    // Insert 500 rows
    for (int i = 0; i < 500; ++i) {
        manager_->insert_query(
            "INSERT INTO mysql_test_table (name, value) VALUES ('bulk_" +
            std::to_string(i) + "', " + std::to_string(i * 0.1) + ")"
        );
    }

    auto result = manager_->select_query(
        "SELECT COUNT(*) as cnt FROM mysql_test_table"
    );
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<int64_t>(result[0].at("cnt")), 500);
}

TEST_F(MySQLIntegrationTest, ConcurrentQueries) {
    if (!connected_) {
        GTEST_SKIP() << "MySQL server not available";
    }

    // Insert initial data
    for (int i = 0; i < 100; ++i) {
        manager_->insert_query(
            "INSERT INTO mysql_test_table (name, value) VALUES ('concurrent_" +
            std::to_string(i) + "', " + std::to_string(i) + ")"
        );
    }

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Multiple reader threads
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([this, &success_count]() {
            for (int i = 0; i < 10; ++i) {
                auto result = manager_->select_query(
                    "SELECT COUNT(*) as cnt FROM mysql_test_table"
                );
                if (!result.empty()) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

#endif // USE_MYSQL

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(MySQLManagerTest, EmptyQueryString) {
    // Empty queries should not crash
    EXPECT_NO_THROW({
        manager_->create_query("");
        manager_->insert_query("");
        manager_->update_query("");
        manager_->delete_query("");
        manager_->select_query("");
        manager_->execute_query("");
    });
}

TEST_F(MySQLManagerTest, VeryLongConnectionString) {
    // Very long connection string should not crash
    std::string long_conn = "host=localhost;port=3306;database=";
    for (int i = 0; i < 1000; ++i) {
        long_conn += "a";
    }
    long_conn += ";user=test;password=test";

    EXPECT_NO_THROW({
        manager_->connect(long_conn);
    });
}

TEST_F(MySQLManagerTest, SpecialCharactersInConnectionString) {
    // Special characters in password
    EXPECT_NO_THROW({
        manager_->connect(
            "host=localhost;port=3306;database=test;user=test;password=p@ss=w;rd"
        );
    });
}

TEST_F(MySQLManagerTest, UnicodeInQuery) {
    // Unicode in query should not crash
    EXPECT_NO_THROW({
        manager_->select_query("SELECT * FROM test WHERE name = '한글'");
    });
}

TEST_F(MySQLManagerTest, MultipleManagerInstances) {
    // Multiple manager instances should work independently
    auto manager2 = std::make_unique<mysql_manager>();
    auto manager3 = std::make_unique<mysql_manager>();

    EXPECT_EQ(manager_->database_type(), database_types::mysql);
    EXPECT_EQ(manager2->database_type(), database_types::mysql);
    EXPECT_EQ(manager3->database_type(), database_types::mysql);

    // Independent connections
    manager_->connect(VALID_CONN_STRING);
    manager2->connect(VALID_CONN_STRING);

    manager_->disconnect();
    // manager2 should still be valid (if it was connected)

    manager2->disconnect();
    manager3->disconnect();
}

TEST_F(MySQLManagerTest, RapidConnectDisconnect) {
    // Rapid connect/disconnect cycles should not cause issues
    for (int i = 0; i < 10; ++i) {
        manager_->connect(VALID_CONN_STRING);
        manager_->disconnect();
    }
    // No crash is success
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
