/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * SQLite Manager Tests (DB-001)
 *
 * Tests for SQLite backend implementation covering:
 * - Connection handling (file database, memory database, invalid paths)
 * - CRUD operations (INSERT, SELECT, UPDATE, DELETE)
 * - Error handling and edge cases
 * - Thread safety with recursive_mutex
 */

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>

#include "database/backends/sqlite/sqlite_manager.h"

using namespace database;

/**
 * @class SQLiteManagerTest
 * @brief Test fixture for SQLite manager tests
 */
class SQLiteManagerTest : public ::testing::Test {
protected:
    std::unique_ptr<sqlite_manager> manager_;
    std::string test_db_path_;

    void SetUp() override {
        manager_ = std::make_unique<sqlite_manager>();
        test_db_path_ = "test_sqlite_" + std::to_string(std::time(nullptr)) + ".db";
    }

    void TearDown() override {
        if (manager_) {
            manager_->disconnect();
        }
        // Clean up test database file
        if (std::filesystem::exists(test_db_path_)) {
            std::filesystem::remove(test_db_path_);
        }
    }

    /**
     * @brief Helper to create a test table
     */
    bool createTestTable() {
        return manager_->execute_query(
            "CREATE TABLE IF NOT EXISTS test_table ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL,"
            "  value REAL,"
            "  active INTEGER DEFAULT 1"
            ")"
        );
    }
};

//=============================================================================
// Database Type Tests
//=============================================================================

TEST_F(SQLiteManagerTest, DatabaseTypeReturnsSQLite) {
    EXPECT_EQ(manager_->database_type(), database_types::sqlite);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(SQLiteManagerTest, ConnectToMemoryDatabase) {
    // In-memory database should always work with USE_SQLITE
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
#else
    // Without USE_SQLITE, connect returns false but shouldn't crash
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, ConnectToFileDatabase) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(test_db_path_));
    EXPECT_TRUE(std::filesystem::exists(test_db_path_));
#else
    EXPECT_FALSE(manager_->connect(test_db_path_));
#endif
}

TEST_F(SQLiteManagerTest, ConnectToInvalidPath) {
#ifdef USE_SQLITE
    // Invalid path should fail to connect
    EXPECT_FALSE(manager_->connect("/nonexistent/directory/database.db"));
#else
    EXPECT_FALSE(manager_->connect("/nonexistent/directory/database.db"));
#endif
}

TEST_F(SQLiteManagerTest, DisconnectWithoutConnection) {
    // Disconnect without prior connection should return false
    EXPECT_FALSE(manager_->disconnect());
}

TEST_F(SQLiteManagerTest, DisconnectAfterConnection) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->disconnect());
    // Second disconnect should return false (already disconnected)
    EXPECT_FALSE(manager_->disconnect());
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
    EXPECT_FALSE(manager_->disconnect());
#endif
}

TEST_F(SQLiteManagerTest, ReconnectAfterDisconnect) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->disconnect());
    EXPECT_TRUE(manager_->connect(":memory:"));
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// CREATE Query Tests
//=============================================================================

TEST_F(SQLiteManagerTest, CreateQueryWithoutConnection) {
    // create_query without connection should return false
    EXPECT_FALSE(manager_->create_query("CREATE TABLE test (id INTEGER)"));
}

TEST_F(SQLiteManagerTest, CreateTableQuery) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->create_query(
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)"
    ));
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, CreateQueryWithInvalidSQL) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    // Invalid SQL should return false
    EXPECT_FALSE(manager_->create_query("INVALID SQL SYNTAX"));
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// INSERT Query Tests
//=============================================================================

TEST_F(SQLiteManagerTest, InsertQueryWithoutConnection) {
    EXPECT_EQ(manager_->insert_query("INSERT INTO test VALUES (1, 'test')"), 0u);
}

TEST_F(SQLiteManagerTest, InsertSingleRow) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    unsigned int affected = manager_->insert_query(
        "INSERT INTO test_table (name, value) VALUES ('item1', 10.5)"
    );
    EXPECT_EQ(affected, 1u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, InsertMultipleRows) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    // Insert first row
    EXPECT_EQ(manager_->insert_query(
        "INSERT INTO test_table (name, value) VALUES ('item1', 10.5)"
    ), 1u);

    // Insert second row
    EXPECT_EQ(manager_->insert_query(
        "INSERT INTO test_table (name, value) VALUES ('item2', 20.5)"
    ), 1u);

    // Insert third row
    EXPECT_EQ(manager_->insert_query(
        "INSERT INTO test_table (name, value) VALUES ('item3', 30.5)"
    ), 1u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, InsertWithInvalidSQL) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    // Invalid insert should return 0
    EXPECT_EQ(manager_->insert_query(
        "INSERT INTO nonexistent_table VALUES (1, 'test')"
    ), 0u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// SELECT Query Tests
//=============================================================================

TEST_F(SQLiteManagerTest, SelectQueryWithoutConnection) {
    auto result = manager_->select_query("SELECT * FROM test");
#ifdef USE_SQLITE
    // Without connection, should return empty
    EXPECT_TRUE(result.empty());
#else
    // Mock mode returns mock data for SELECT queries
    EXPECT_EQ(result.size(), 1u);
#endif
}

TEST_F(SQLiteManagerTest, SelectFromEmptyTable) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    auto result = manager_->select_query("SELECT * FROM test_table");
    EXPECT_TRUE(result.empty());
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, SelectAllColumns) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query(
        "INSERT INTO test_table (name, value, active) VALUES ('test_item', 42.5, 1)"
    );

    auto result = manager_->select_query("SELECT * FROM test_table");
    ASSERT_EQ(result.size(), 1u);

    const auto& row = result[0];
    EXPECT_TRUE(row.find("id") != row.end());
    EXPECT_TRUE(row.find("name") != row.end());
    EXPECT_TRUE(row.find("value") != row.end());
    EXPECT_TRUE(row.find("active") != row.end());

    // Check value types
    EXPECT_TRUE(std::holds_alternative<std::string>(row.at("name")));
    EXPECT_EQ(std::get<std::string>(row.at("name")), "test_item");
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
    // Mock mode returns mock data
    auto result = manager_->select_query("SELECT * FROM test_table");
    EXPECT_EQ(result.size(), 1u);
#endif
}

TEST_F(SQLiteManagerTest, SelectWithCondition) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item1', 10.0)");
    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item2', 20.0)");
    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item3', 30.0)");

    auto result = manager_->select_query(
        "SELECT * FROM test_table WHERE value > 15.0"
    );
    EXPECT_EQ(result.size(), 2u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, SelectWithInvalidSQL) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    auto result = manager_->select_query("SELECT * FROM nonexistent_table");
    EXPECT_TRUE(result.empty());
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, SelectIntegerTypes) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->execute_query(
        "CREATE TABLE int_test (val INTEGER)"
    ));
    manager_->insert_query("INSERT INTO int_test VALUES (12345)");

    auto result = manager_->select_query("SELECT val FROM int_test");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<int64_t>(result[0].at("val")));
    EXPECT_EQ(std::get<int64_t>(result[0].at("val")), 12345);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, SelectFloatTypes) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->execute_query(
        "CREATE TABLE float_test (val REAL)"
    ));
    manager_->insert_query("INSERT INTO float_test VALUES (3.14159)");

    auto result = manager_->select_query("SELECT val FROM float_test");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<double>(result[0].at("val")));
    EXPECT_NEAR(std::get<double>(result[0].at("val")), 3.14159, 0.00001);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, SelectNullValues) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->execute_query(
        "CREATE TABLE null_test (val TEXT)"
    ));
    manager_->insert_query("INSERT INTO null_test VALUES (NULL)");

    auto result = manager_->select_query("SELECT val FROM null_test");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[0].at("val")));
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// UPDATE Query Tests
//=============================================================================

TEST_F(SQLiteManagerTest, UpdateQueryWithoutConnection) {
    EXPECT_EQ(manager_->update_query("UPDATE test SET name = 'new'"), 0u);
}

TEST_F(SQLiteManagerTest, UpdateSingleRow) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('old_name', 10.0)");

    unsigned int affected = manager_->update_query(
        "UPDATE test_table SET name = 'new_name' WHERE id = 1"
    );
    EXPECT_EQ(affected, 1u);

    auto result = manager_->select_query("SELECT name FROM test_table WHERE id = 1");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<std::string>(result[0].at("name")), "new_name");
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, UpdateMultipleRows) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query("INSERT INTO test_table (name, value, active) VALUES ('item1', 10.0, 1)");
    manager_->insert_query("INSERT INTO test_table (name, value, active) VALUES ('item2', 20.0, 1)");
    manager_->insert_query("INSERT INTO test_table (name, value, active) VALUES ('item3', 30.0, 0)");

    unsigned int affected = manager_->update_query(
        "UPDATE test_table SET value = 100.0 WHERE active = 1"
    );
    EXPECT_EQ(affected, 2u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, UpdateNoMatchingRows) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item1', 10.0)");

    unsigned int affected = manager_->update_query(
        "UPDATE test_table SET value = 100.0 WHERE id = 999"
    );
    EXPECT_EQ(affected, 0u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// DELETE Query Tests
//=============================================================================

TEST_F(SQLiteManagerTest, DeleteQueryWithoutConnection) {
    EXPECT_EQ(manager_->delete_query("DELETE FROM test WHERE id = 1"), 0u);
}

TEST_F(SQLiteManagerTest, DeleteSingleRow) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item1', 10.0)");
    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item2', 20.0)");

    unsigned int affected = manager_->delete_query(
        "DELETE FROM test_table WHERE id = 1"
    );
    EXPECT_EQ(affected, 1u);

    auto result = manager_->select_query("SELECT COUNT(*) as cnt FROM test_table");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<int64_t>(result[0].at("cnt")), 1);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, DeleteAllRows) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item1', 10.0)");
    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item2', 20.0)");
    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('item3', 30.0)");

    unsigned int affected = manager_->delete_query("DELETE FROM test_table");
    EXPECT_EQ(affected, 3u);

    auto result = manager_->select_query("SELECT * FROM test_table");
    EXPECT_TRUE(result.empty());
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// execute_query Tests
//=============================================================================

TEST_F(SQLiteManagerTest, ExecuteQueryWithoutConnection) {
#ifdef USE_SQLITE
    // Without connection, should return false
    EXPECT_FALSE(manager_->execute_query("CREATE TABLE test (id INTEGER)"));
#else
    // Mock mode returns true for execute_query
    EXPECT_TRUE(manager_->execute_query("CREATE TABLE test (id INTEGER)"));
#endif
}

TEST_F(SQLiteManagerTest, ExecuteCreateTable) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->execute_query(
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)"
    ));

    // Verify table exists
    auto result = manager_->select_query(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='users'"
    );
    EXPECT_EQ(result.size(), 1u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
    // Mock mode returns true for execute_query
    EXPECT_TRUE(manager_->execute_query("CREATE TABLE test (id INTEGER)"));
#endif
}

TEST_F(SQLiteManagerTest, ExecuteDropTable) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->execute_query("CREATE TABLE temp_table (id INTEGER)"));
    EXPECT_TRUE(manager_->execute_query("DROP TABLE temp_table"));

    auto result = manager_->select_query(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='temp_table'"
    );
    EXPECT_TRUE(result.empty());
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, ExecuteInvalidQuery) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_FALSE(manager_->execute_query("INVALID SQL STATEMENT"));
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// Transaction Tests
//=============================================================================

TEST_F(SQLiteManagerTest, TransactionCommit) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    EXPECT_TRUE(manager_->execute_query("BEGIN TRANSACTION"));
    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('trans_item', 50.0)");
    EXPECT_TRUE(manager_->execute_query("COMMIT"));

    auto result = manager_->select_query("SELECT * FROM test_table WHERE name = 'trans_item'");
    EXPECT_EQ(result.size(), 1u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, TransactionRollback) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    EXPECT_TRUE(manager_->execute_query("BEGIN TRANSACTION"));
    manager_->insert_query("INSERT INTO test_table (name, value) VALUES ('rollback_item', 50.0)");
    EXPECT_TRUE(manager_->execute_query("ROLLBACK"));

    auto result = manager_->select_query("SELECT * FROM test_table WHERE name = 'rollback_item'");
    EXPECT_TRUE(result.empty());
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// Thread Safety Tests (recursive_mutex verification)
//=============================================================================

TEST_F(SQLiteManagerTest, ConcurrentReads) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    // Insert test data
    for (int i = 0; i < 100; ++i) {
        manager_->insert_query(
            "INSERT INTO test_table (name, value) VALUES ('item" +
            std::to_string(i) + "', " + std::to_string(i * 1.5) + ")"
        );
    }

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Spawn multiple reader threads
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([this, &success_count]() {
            for (int i = 0; i < 10; ++i) {
                auto result = manager_->select_query("SELECT COUNT(*) as cnt FROM test_table");
                if (!result.empty() && std::get<int64_t>(result[0].at("cnt")) == 100) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 100);
#else
    GTEST_SKIP() << "SQLite support not compiled";
#endif
}

TEST_F(SQLiteManagerTest, ConcurrentWritesAndReads) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    std::atomic<int> write_count{0};
    std::atomic<int> read_count{0};
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([this, &write_count, t]() {
            for (int i = 0; i < 20; ++i) {
                unsigned int result = manager_->insert_query(
                    "INSERT INTO test_table (name, value) VALUES ('thread" +
                    std::to_string(t) + "_item" + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ")"
                );
                if (result > 0) {
                    write_count++;
                }
            }
        });
    }

    // Reader threads
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([this, &read_count, &stop]() {
            while (!stop.load()) {
                auto result = manager_->select_query("SELECT COUNT(*) as cnt FROM test_table");
                if (!result.empty()) {
                    read_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Wait for writers to complete
    for (int i = 0; i < 3; ++i) {
        threads[i].join();
    }

    stop.store(true);

    // Wait for readers to complete
    for (size_t i = 3; i < threads.size(); ++i) {
        threads[i].join();
    }

    EXPECT_EQ(write_count.load(), 60); // 3 threads * 20 inserts
    EXPECT_GT(read_count.load(), 0);

    // Verify final data integrity
    auto result = manager_->select_query("SELECT COUNT(*) as cnt FROM test_table");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<int64_t>(result[0].at("cnt")), 60);
#else
    GTEST_SKIP() << "SQLite support not compiled";
#endif
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SQLiteManagerTest, EmptyQueryString) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_FALSE(manager_->execute_query(""));
    EXPECT_FALSE(manager_->create_query(""));
    EXPECT_EQ(manager_->insert_query(""), 0u);
    EXPECT_EQ(manager_->update_query(""), 0u);
    EXPECT_EQ(manager_->delete_query(""), 0u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, SpecialCharactersInData) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    // Insert data with special characters (using SQL escaping)
    manager_->insert_query(
        "INSERT INTO test_table (name, value) VALUES ('test''s data', 10.0)"
    );

    auto result = manager_->select_query("SELECT name FROM test_table");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<std::string>(result[0].at("name")), "test's data");
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, UnicodeData) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    manager_->insert_query(
        "INSERT INTO test_table (name, value) VALUES ('한글테스트', 10.0)"
    );

    auto result = manager_->select_query("SELECT name FROM test_table");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<std::string>(result[0].at("name")), "한글테스트");
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

TEST_F(SQLiteManagerTest, LargeDataSet) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(createTestTable());

    // Insert 1000 rows
    for (int i = 0; i < 1000; ++i) {
        manager_->insert_query(
            "INSERT INTO test_table (name, value) VALUES ('bulk_item_" +
            std::to_string(i) + "', " + std::to_string(i * 0.1) + ")"
        );
    }

    auto result = manager_->select_query("SELECT COUNT(*) as cnt FROM test_table");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<int64_t>(result[0].at("cnt")), 1000);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// BLOB Data Tests
//=============================================================================

TEST_F(SQLiteManagerTest, BlobDataHandling) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));
    EXPECT_TRUE(manager_->execute_query(
        "CREATE TABLE blob_test (id INTEGER PRIMARY KEY, data BLOB)"
    ));

    // Insert blob data as hex
    manager_->insert_query("INSERT INTO blob_test VALUES (1, X'48454C4C4F')");

    auto result = manager_->select_query("SELECT data FROM blob_test WHERE id = 1");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<std::string>(result[0].at("data")));
    EXPECT_EQ(std::get<std::string>(result[0].at("data")), "HELLO");
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

//=============================================================================
// Foreign Key Tests
//=============================================================================

TEST_F(SQLiteManagerTest, ForeignKeyConstraint) {
#ifdef USE_SQLITE
    EXPECT_TRUE(manager_->connect(":memory:"));

    // Create parent table
    EXPECT_TRUE(manager_->execute_query(
        "CREATE TABLE parent (id INTEGER PRIMARY KEY, name TEXT)"
    ));

    // Create child table with foreign key
    EXPECT_TRUE(manager_->execute_query(
        "CREATE TABLE child ("
        "  id INTEGER PRIMARY KEY,"
        "  parent_id INTEGER,"
        "  FOREIGN KEY (parent_id) REFERENCES parent(id)"
        ")"
    ));

    // Insert parent
    manager_->insert_query("INSERT INTO parent VALUES (1, 'parent1')");

    // Insert valid child
    EXPECT_EQ(manager_->insert_query("INSERT INTO child VALUES (1, 1)"), 1u);

    // Insert child with invalid foreign key (should fail with FK enabled)
    // Note: This depends on PRAGMA foreign_keys = ON being set
    unsigned int result = manager_->insert_query("INSERT INTO child VALUES (2, 999)");
    EXPECT_EQ(result, 0u);
#else
    EXPECT_FALSE(manager_->connect(":memory:"));
#endif
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
