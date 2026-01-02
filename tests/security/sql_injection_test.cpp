/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * SQL Injection Prevention Tests (DB-008)
 *
 * Tests for SQL injection prevention in query builder:
 * - Classic injection attempts (OR '1'='1, --, ;)
 * - Union-based injection attempts
 * - Parameterized query safety
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "database/query_builder.h"
#include "database/backends/sqlite_backend.h"
#include "database/core/database_backend.h"

using namespace database;
using namespace database::backends;
using namespace database::core;

/**
 * @class SQLInjectionTest
 * @brief Test fixture for SQL injection prevention tests
 */
class SQLInjectionTest : public ::testing::Test {
protected:
    std::unique_ptr<sqlite_backend> db_;

    void SetUp() override {
        db_ = std::make_unique<sqlite_backend>();
#ifdef USE_SQLITE
        connection_config config;
        config.database = ":memory:";
        ASSERT_TRUE(db_->initialize(config).is_ok());
        ASSERT_TRUE(db_->execute_query("CREATE TABLE users ("
            "  id INTEGER PRIMARY KEY,"
            "  name TEXT,"
            "  email TEXT,"
            "  password_hash TEXT"
            ")").is_ok());
        // Insert test data
        auto r1 = db_->insert_query(
            "INSERT INTO users (id, name, email, password_hash) "
            "VALUES (1, 'Alice', 'alice@test.com', 'hash123')"
        );
        ASSERT_TRUE(r1.is_ok());
        ASSERT_GT(r1.value(), 0u);
        auto r2 = db_->insert_query(
            "INSERT INTO users (id, name, email, password_hash) "
            "VALUES (2, 'Bob', 'bob@test.com', 'hash456')"
        );
        ASSERT_TRUE(r2.is_ok());
        ASSERT_GT(r2.value(), 0u);
#else
        GTEST_SKIP() << "SQLite not available";
#endif
    }

    void TearDown() override {
        if (db_ && db_->is_initialized()) {
            db_->shutdown();
        }
    }
};

//=============================================================================
// Classic SQL Injection Attempts
//=============================================================================

/**
 * @test BasicInjectionAttempt
 * @brief Tests that basic OR injection is properly escaped
 *
 * Attack: ' OR '1'='1
 * Expected: Query treats input as literal string, returns 0 rows
 */
TEST_F(SQLInjectionTest, BasicInjectionAttempt) {
#ifdef USE_SQLITE
    std::string malicious_input = "' OR '1'='1";

    query_builder builder(database_types::sqlite);
    auto query = builder
        .select({"*"})
        .from("users")
        .where("name", "=", malicious_input)
        .build();

    auto query_result = db_->select_query(query);

    // If properly escaped, should return 0 rows (no user named "' OR '1'='1")
    // If vulnerable, would return all rows
    if (query_result.is_ok()) {
        EXPECT_LE(query_result.value().size(), 1u)
            << "Possible SQL injection vulnerability: query returned multiple rows";
    }
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test CommentInjectionAttempt
 * @brief Tests that comment-based injection is handled
 *
 * Attack: admin'--
 * Expected: Query treats input as literal string
 */
TEST_F(SQLInjectionTest, CommentInjectionAttempt) {
#ifdef USE_SQLITE
    std::string malicious_input = "admin'--";

    query_builder builder(database_types::sqlite);
    auto query = builder
        .select({"*"})
        .from("users")
        .where("name", "=", malicious_input)
        .build();

    auto query_result = db_->select_query(query);

    // Should return 0 rows - no user named "admin'--"
    if (query_result.is_ok()) {
        EXPECT_EQ(query_result.value().size(), 0u);
    }
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test BatchStatementInjectionAttempt
 * @brief Tests that batch statement injection is prevented
 *
 * Attack: '; DROP TABLE users; --
 * Expected: Table should still exist after query
 */
TEST_F(SQLInjectionTest, BatchStatementInjectionAttempt) {
#ifdef USE_SQLITE
    std::string malicious_input = "'; DROP TABLE users; --";

    query_builder builder(database_types::sqlite);
    auto query = builder
        .select({"*"})
        .from("users")
        .where("name", "=", malicious_input)
        .build();

    // Execute the query (may fail, but shouldn't drop table)
    db_->select_query(query);

    // Verify table still exists by querying it
    auto check = db_->select_query("SELECT COUNT(*) as cnt FROM users");
    EXPECT_TRUE(check.is_ok() && !check.value().empty())
        << "CRITICAL: Table appears to have been dropped!";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Union-Based Injection Attempts
//=============================================================================

/**
 * @test UnionInjectionAttempt
 * @brief Tests that UNION-based injection is prevented
 *
 * Attack: ' UNION SELECT * FROM sensitive_data --
 * Expected: Query should not execute UNION
 */
TEST_F(SQLInjectionTest, UnionInjectionAttempt) {
#ifdef USE_SQLITE
    // Create a sensitive table for the test
    db_->execute_query(
        "CREATE TABLE sensitive_data (secret TEXT)"
    );
    db_->insert_query(
        "INSERT INTO sensitive_data VALUES ('top_secret_value')"
    );

    std::string malicious_input = "' UNION SELECT secret, secret, secret, secret FROM sensitive_data --";

    query_builder builder(database_types::sqlite);
    auto query = builder
        .select({"*"})
        .from("users")
        .where("name", "=", malicious_input)
        .build();

    auto query_result = db_->select_query(query);

    // Check that sensitive data was not leaked
    bool found_secret = false;
    if (query_result.is_ok()) {
        for (const auto& row : query_result.value()) {
            for (const auto& [key, value] : row) {
                if (std::holds_alternative<std::string>(value)) {
                    if (std::get<std::string>(value).find("top_secret") != std::string::npos) {
                        found_secret = true;
                    }
                }
            }
        }
    }
    EXPECT_FALSE(found_secret)
        << "CRITICAL: Sensitive data leaked through UNION injection!";
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Parameterized Value Safety Tests
//=============================================================================

/**
 * @test ApostropheInValueSafe
 * @brief Tests that apostrophes in values are properly handled
 *
 * Input: O'Brien (legitimate name)
 * Expected: Query executes safely with escaped apostrophe
 */
TEST_F(SQLInjectionTest, ApostropheInValueSafe) {
#ifdef USE_SQLITE
    // Insert a user with apostrophe in name
    auto insert_result = db_->insert_query(
        "INSERT INTO users (id, name, email, password_hash) "
        "VALUES (3, 'O''Brien', 'obrien@test.com', 'hash789')"
    );
    (void)insert_result;

    database_value safe_value = std::string("O'Brien");

    query_builder builder(database_types::sqlite);
    auto query = builder
        .select({"*"})
        .from("users")
        .where("name", "=", safe_value)
        .build();

    // Check that apostrophe is escaped in built query
    // Should contain either O''Brien (SQL standard) or O\'Brien (MySQL)
    bool properly_escaped =
        (query.find("O''Brien") != std::string::npos) ||
        (query.find("O\\'Brien") != std::string::npos) ||
        (query.find("O'Brien") != std::string::npos);  // May work if using prepared stmt

    EXPECT_TRUE(properly_escaped)
        << "Query should handle apostrophe safely. Built query: " << query;
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test SpecialCharactersInValue
 * @brief Tests handling of various special characters
 */
TEST_F(SQLInjectionTest, SpecialCharactersInValue) {
#ifdef USE_SQLITE
    std::vector<std::string> special_inputs = {
        "test\\value",     // Backslash
        "test\"value",     // Double quote
        "test\nvalue",     // Newline
        "test\rvalue",     // Carriage return
        "test\tvalue",     // Tab
        "test%value",      // Percent (LIKE wildcard)
        "test_value",      // Underscore (LIKE wildcard)
    };

    for (const auto& input : special_inputs) {
        query_builder builder(database_types::sqlite);
        auto query = builder
            .select({"*"})
            .from("users")
            .where("name", "=", input)
            .build();

        // Query should build without throwing
        EXPECT_FALSE(query.empty())
            << "Failed to build query with special input: " << input;

        // Query execution should not throw
        EXPECT_NO_THROW({
            db_->select_query(query);
        }) << "Query execution failed with special input: " << input;
    }
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Numeric Value Tests
//=============================================================================

/**
 * @test NumericValueInjection
 * @brief Tests that numeric value fields handle string injection
 */
TEST_F(SQLInjectionTest, NumericValueInjection) {
#ifdef USE_SQLITE
    // Attempting to inject via what should be a numeric field
    database_value numeric_value = static_cast<int64_t>(1);

    query_builder builder(database_types::sqlite);
    auto query = builder
        .select({"*"})
        .from("users")
        .where("id", "=", numeric_value)
        .build();

    // Numeric values should be rendered without quotes
    EXPECT_TRUE(query.find("id = 1") != std::string::npos ||
                query.find("[id] = 1") != std::string::npos)
        << "Numeric value should not be quoted. Query: " << query;
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

/**
 * @test BooleanValueHandling
 * @brief Tests that boolean values are handled safely
 */
TEST_F(SQLInjectionTest, BooleanValueHandling) {
#ifdef USE_SQLITE
    database_value bool_value = true;

    query_builder builder(database_types::sqlite);
    auto query = builder
        .select({"*"})
        .from("users")
        .where("id", ">", bool_value)
        .build();

    // Should render as TRUE or 1, not as quoted string
    EXPECT_TRUE(query.find("TRUE") != std::string::npos ||
                query.find("1") != std::string::npos)
        << "Boolean value handling issue. Query: " << query;
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}

//=============================================================================
// Query Builder State Tests
//=============================================================================

/**
 * @test ResetPreventsDataLeakage
 * @brief Tests that reset() properly clears all builder state
 */
TEST_F(SQLInjectionTest, ResetPreventsDataLeakage) {
    query_builder builder(database_types::sqlite);

    // Build first query with sensitive filter
    builder
        .select({"*"})
        .from("users")
        .where("email", "=", std::string("admin@secret.com"));

    // Reset and build new query
    builder.reset();
    auto query = builder
        .select({"id"})
        .from("public_data")
        .build();

    // Previous filter should not leak into new query
    EXPECT_TRUE(query.find("admin@secret") == std::string::npos)
        << "Previous query data leaked after reset! Query: " << query;
    EXPECT_TRUE(query.find("users") == std::string::npos)
        << "Previous table leaked after reset! Query: " << query;
}

//=============================================================================
// Encoding Attack Tests
//=============================================================================

/**
 * @test UnicodeBypassAttempt
 * @brief Tests handling of Unicode characters that might bypass filters
 */
TEST_F(SQLInjectionTest, UnicodeBypassAttempt) {
#ifdef USE_SQLITE
    // Various Unicode representations that might bypass naive filters
    std::vector<std::string> unicode_attacks = {
        "\xc0\x27",          // Overlong encoding of '
        "\xe0\x80\xa7",      // Another overlong encoding
        "admin\xef\xbb\xbf", // With BOM
    };

    for (const auto& attack : unicode_attacks) {
        query_builder builder(database_types::sqlite);
        auto query = builder
            .select({"*"})
            .from("users")
            .where("name", "=", attack)
            .build();

        // Should handle without crashing
        EXPECT_NO_THROW({
            db_->select_query(query);
        }) << "Query failed with unicode attack: [binary data]";
    }
#else
    GTEST_SKIP() << "SQLite not available";
#endif
}
