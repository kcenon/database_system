// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file sql_dialect_test.cpp
 * @brief Unit tests for SQL dialect abstraction (Issue #332)
 *
 * Tests for database-specific SQL syntax:
 * - Placeholder style ($1 vs ? vs ?1)
 * - Identifier quoting ("col" vs `col`)
 * - String escaping
 * - RETURNING clause support
 * - UPSERT clause generation
 */

#include <gtest/gtest.h>
#include <kcenon/database/query_builder/sql_dialect.h>
#include <memory>

namespace database::query::tests
{

//=============================================================================
// PostgreSQL Dialect Tests
//=============================================================================

class PostgreSQLDialectTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        dialect_ = sql_dialect::create(database_types::postgres);
    }

    std::unique_ptr<sql_dialect> dialect_;
};

TEST_F(PostgreSQLDialectTest, PlaceholderStyle)
{
    EXPECT_EQ(dialect_->placeholder(1), "$1");
    EXPECT_EQ(dialect_->placeholder(2), "$2");
    EXPECT_EQ(dialect_->placeholder(10), "$10");
    EXPECT_EQ(dialect_->placeholder(100), "$100");
}

TEST_F(PostgreSQLDialectTest, QuoteIdentifier)
{
    EXPECT_EQ(dialect_->quote_identifier("users"), "\"users\"");
    EXPECT_EQ(dialect_->quote_identifier("user_name"), "\"user_name\"");
    EXPECT_EQ(dialect_->quote_identifier("SELECT"), "\"SELECT\"");  // Reserved word
}

TEST_F(PostgreSQLDialectTest, QuoteIdentifierWithSpecialChars)
{
    // Double quotes should be escaped by doubling
    EXPECT_EQ(dialect_->quote_identifier("user\"name"), "\"user\"\"name\"");
}

TEST_F(PostgreSQLDialectTest, EscapeString)
{
    EXPECT_EQ(dialect_->escape_string("hello"), "hello");
    EXPECT_EQ(dialect_->escape_string("it's"), "it''s");  // Single quote doubled
    EXPECT_EQ(dialect_->escape_string("back\\slash"), "back\\\\slash");  // Backslash escaped
    EXPECT_EQ(dialect_->escape_string("O'Brien's"), "O''Brien''s");
}

TEST_F(PostgreSQLDialectTest, ReturningClause)
{
    EXPECT_EQ(dialect_->returning_clause("id"), " RETURNING \"id\"");
    EXPECT_EQ(dialect_->returning_clause(""), " RETURNING *");
}

TEST_F(PostgreSQLDialectTest, UpsertClause)
{
    std::vector<std::string> conflict_cols = {"id"};
    std::vector<std::string> update_cols = {"name", "email"};

    std::string result = dialect_->upsert_clause(conflict_cols, update_cols);

    EXPECT_TRUE(result.find("ON CONFLICT") != std::string::npos);
    EXPECT_TRUE(result.find("\"id\"") != std::string::npos);
    EXPECT_TRUE(result.find("DO UPDATE SET") != std::string::npos);
    EXPECT_TRUE(result.find("EXCLUDED") != std::string::npos);
}

TEST_F(PostgreSQLDialectTest, UpsertClauseDoNothing)
{
    std::vector<std::string> conflict_cols = {"id"};
    std::vector<std::string> update_cols;  // Empty = DO NOTHING

    std::string result = dialect_->upsert_clause(conflict_cols, update_cols);

    EXPECT_TRUE(result.find("DO NOTHING") != std::string::npos);
}

TEST_F(PostgreSQLDialectTest, UpsertClauseMultipleConflictColumns)
{
    std::vector<std::string> conflict_cols = {"tenant_id", "user_id"};
    std::vector<std::string> update_cols = {"updated_at"};

    std::string result = dialect_->upsert_clause(conflict_cols, update_cols);

    EXPECT_TRUE(result.find("\"tenant_id\"") != std::string::npos);
    EXPECT_TRUE(result.find("\"user_id\"") != std::string::npos);
}

TEST_F(PostgreSQLDialectTest, SupportsFeatureReturning)
{
    EXPECT_TRUE(dialect_->supports_feature("returning"));
    EXPECT_TRUE(dialect_->supports_feature("upsert"));
    EXPECT_TRUE(dialect_->supports_feature("full_outer_join"));
}

//=============================================================================
// SQLite Dialect Tests
//=============================================================================

class SQLiteDialectTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        dialect_ = sql_dialect::create(database_types::sqlite);
    }

    std::unique_ptr<sql_dialect> dialect_;
};

TEST_F(SQLiteDialectTest, PlaceholderStyle)
{
    // SQLite uses ?N numbered placeholders
    EXPECT_EQ(dialect_->placeholder(1), "?1");
    EXPECT_EQ(dialect_->placeholder(2), "?2");
    EXPECT_EQ(dialect_->placeholder(10), "?10");
}

TEST_F(SQLiteDialectTest, QuoteIdentifier)
{
    EXPECT_EQ(dialect_->quote_identifier("users"), "\"users\"");
    EXPECT_EQ(dialect_->quote_identifier("user_name"), "\"user_name\"");
    EXPECT_EQ(dialect_->quote_identifier("SELECT"), "\"SELECT\"");
}

TEST_F(SQLiteDialectTest, QuoteIdentifierWithQuotes)
{
    // Double quotes should be escaped by doubling
    EXPECT_EQ(dialect_->quote_identifier("user\"name"), "\"user\"\"name\"");
}

TEST_F(SQLiteDialectTest, EscapeString)
{
    EXPECT_EQ(dialect_->escape_string("hello"), "hello");
    EXPECT_EQ(dialect_->escape_string("it's"), "it''s");  // Single quote doubled
    EXPECT_EQ(dialect_->escape_string("O'Brien's"), "O''Brien''s");
}

TEST_F(SQLiteDialectTest, ReturningClause)
{
    // SQLite 3.35+ supports RETURNING
    EXPECT_EQ(dialect_->returning_clause("id"), " RETURNING \"id\"");
    EXPECT_EQ(dialect_->returning_clause(""), " RETURNING *");
}

TEST_F(SQLiteDialectTest, UpsertClause)
{
    std::vector<std::string> conflict_cols = {"id"};
    std::vector<std::string> update_cols = {"name", "email"};

    std::string result = dialect_->upsert_clause(conflict_cols, update_cols);

    EXPECT_TRUE(result.find("ON CONFLICT") != std::string::npos);
    EXPECT_TRUE(result.find("\"id\"") != std::string::npos);
    EXPECT_TRUE(result.find("DO UPDATE SET") != std::string::npos);
    EXPECT_TRUE(result.find("excluded") != std::string::npos);  // SQLite uses lowercase
}

TEST_F(SQLiteDialectTest, UpsertClauseDoNothing)
{
    std::vector<std::string> conflict_cols = {"id"};
    std::vector<std::string> update_cols;

    std::string result = dialect_->upsert_clause(conflict_cols, update_cols);

    EXPECT_TRUE(result.find("DO NOTHING") != std::string::npos);
}

TEST_F(SQLiteDialectTest, SupportsFeatureReturning)
{
    EXPECT_TRUE(dialect_->supports_feature("returning"));
    EXPECT_TRUE(dialect_->supports_feature("upsert"));
    EXPECT_FALSE(dialect_->supports_feature("full_outer_join"));
}

TEST_F(SQLiteDialectTest, LimitClauseSyntax)
{
    EXPECT_EQ(dialect_->limit_clause(10, 0), "LIMIT 10");
    EXPECT_EQ(dialect_->limit_clause(10, 20), "LIMIT 10 OFFSET 20");
}

//=============================================================================
// Factory Method Tests
//=============================================================================

TEST(SqlDialectFactoryTest, CreatePostgreSQLDialect)
{
    auto dialect = sql_dialect::create(database_types::postgres);
    ASSERT_NE(dialect, nullptr);
    EXPECT_EQ(dialect->placeholder(1), "$1");
}

TEST(SqlDialectFactoryTest, CreateSQLiteDialect)
{
    auto dialect = sql_dialect::create(database_types::sqlite);
    ASSERT_NE(dialect, nullptr);
    EXPECT_EQ(dialect->placeholder(1), "?1");
}

TEST(SqlDialectFactoryTest, CreateUnsupportedDialectThrows)
{
    EXPECT_THROW(
        sql_dialect::create(database_types::mongodb),
        std::invalid_argument
    );
    EXPECT_THROW(
        sql_dialect::create(database_types::redis),
        std::invalid_argument
    );
}

//=============================================================================
// Cross-Dialect Comparison Tests
//=============================================================================

class CrossDialectTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        postgres_ = sql_dialect::create(database_types::postgres);
        sqlite_ = sql_dialect::create(database_types::sqlite);
    }

    std::unique_ptr<sql_dialect> postgres_;
    std::unique_ptr<sql_dialect> sqlite_;
};

TEST_F(CrossDialectTest, PlaceholderStylesDiffer)
{
    // Both dialects should produce different placeholders
    EXPECT_NE(postgres_->placeholder(1), sqlite_->placeholder(1));
}

TEST_F(CrossDialectTest, QuoteIdentifierStyles)
{
    std::string col = "user_id";

    // PostgreSQL and SQLite use double quotes
    EXPECT_EQ(postgres_->quote_identifier(col), "\"user_id\"");
    EXPECT_EQ(sqlite_->quote_identifier(col), "\"user_id\"");
}

TEST_F(CrossDialectTest, ConcatOperatorDiffers)
{
    // PostgreSQL and SQLite use ||
    EXPECT_EQ(postgres_->concat_operator(), "||");
    EXPECT_EQ(sqlite_->concat_operator(), "||");
}

TEST_F(CrossDialectTest, AutoIncrementSyntax)
{
    EXPECT_EQ(postgres_->auto_increment(), "SERIAL");
    EXPECT_EQ(sqlite_->auto_increment(), "AUTOINCREMENT");
}

TEST_F(CrossDialectTest, CurrentTimestampFunction)
{
    EXPECT_EQ(postgres_->current_timestamp(), "CURRENT_TIMESTAMP");
    EXPECT_EQ(sqlite_->current_timestamp(), "CURRENT_TIMESTAMP");
}

} // namespace database::query::tests
