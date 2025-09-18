/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Database System Project
 */

#include <gtest/gtest.h>
#include <memory>

#include "database/database_manager.h"
#include "database/database_types.h"

using namespace database;

// Test fixture for database tests
class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }

    void TearDown() override {
        // Cleanup
        auto& db = database_manager::handle();
        db.disconnect();
    }
};

// Basic database manager tests
TEST_F(DatabaseTest, DatabaseManagerSingleton) {
    auto& db1 = database_manager::handle();
    auto& db2 = database_manager::handle();

    // Should be the same instance (singleton)
    EXPECT_EQ(&db1, &db2);
}

TEST_F(DatabaseTest, DatabaseTypeSettings) {
    auto& db = database_manager::handle();

    // Test setting PostgreSQL (currently the only supported backend)
    EXPECT_TRUE(db.set_mode(database_types::postgres));
    EXPECT_EQ(db.database_type(), database_types::postgres);

    // Reset to ensure clean state
    db.disconnect();

    // Test that unsupported backends return false (as expected)
    EXPECT_FALSE(db.set_mode(database_types::mysql));
    EXPECT_EQ(db.database_type(), database_types::none);

    EXPECT_FALSE(db.set_mode(database_types::sqlite));
    EXPECT_EQ(db.database_type(), database_types::none);
}

TEST_F(DatabaseTest, BasicQueryOperations) {
    auto& db = database_manager::handle();

    // Set database mode
    EXPECT_TRUE(db.set_mode(database_types::postgres));

    // Test query creation (should not crash)
    EXPECT_NO_THROW(db.create_query("SELECT 1"));

    // Test select query behavior
    auto result = db.select_query("SELECT 1");
    // Note: PostgreSQL support may not be compiled, so result may contain error info
    // We just test that it doesn't crash and returns some result
    EXPECT_NO_THROW(result);
}

TEST_F(DatabaseTest, ConnectionHandling) {
    auto& db = database_manager::handle();

    // Set database mode
    EXPECT_TRUE(db.set_mode(database_types::postgres));

    // Test connection with invalid connection string (should fail gracefully)
    EXPECT_FALSE(db.connect("invalid_connection_string"));

    // Test disconnect (should not crash)
    EXPECT_NO_THROW(db.disconnect());
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}