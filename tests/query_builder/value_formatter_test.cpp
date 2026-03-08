// BSD 3-Clause License
// Copyright (c) 2025
// All rights reserved.

/**
 * @file value_formatter_test.cpp
 * @brief Unit tests for value_formatter
 */

#include <gtest/gtest.h>
#include "database/query_builder/value_formatter.h"

using namespace database;
using namespace database::query;

class ValueFormatterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ValueFormatterTest, PostgreSQLStringEscaping) {
    value_formatter fmt(database_types::postgres);

    // Test single quote escaping
    std::string input = "O'Brien";
    std::string escaped = fmt.escape_string(input);
    EXPECT_EQ(escaped, "O''Brien");

    // Test backslash escaping
    input = "path\\to\\file";
    escaped = fmt.escape_string(input);
    EXPECT_TRUE(escaped.find("\\\\") != std::string::npos);
}

TEST_F(ValueFormatterTest, SQLiteStringEscaping) {
    value_formatter fmt(database_types::sqlite);

    // Test single quote escaping
    std::string input = "O'Brien";
    std::string escaped = fmt.escape_string(input);
    EXPECT_EQ(escaped, "O''Brien");
}

TEST_F(ValueFormatterTest, IdentifierQuoting) {
    value_formatter pg_fmt(database_types::postgres);
    EXPECT_EQ(pg_fmt.escape_identifier("table"), "\"table\"");

    value_formatter sqlite_fmt(database_types::sqlite);
    EXPECT_EQ(sqlite_fmt.escape_identifier("table"), "\"table\"");
}

TEST_F(ValueFormatterTest, BooleanLiterals) {
    value_formatter pg_fmt(database_types::postgres);
    EXPECT_EQ(pg_fmt.bool_literal(true), "TRUE");
    EXPECT_EQ(pg_fmt.bool_literal(false), "FALSE");

}

TEST_F(ValueFormatterTest, NullLiteral) {
    value_formatter fmt(database_types::postgres);
    EXPECT_EQ(fmt.null_literal(), "NULL");
}

TEST_F(ValueFormatterTest, FormatString) {
    value_formatter fmt(database_types::postgres);
    std::string value = "test";
    std::string formatted = fmt.format(value);
    EXPECT_TRUE(formatted.find("'") != std::string::npos);
}

TEST_F(ValueFormatterTest, FormatInteger) {
    value_formatter fmt(database_types::postgres);
    int value = 42;
    std::string formatted = fmt.format(value);
    EXPECT_EQ(formatted, "42");
}

TEST_F(ValueFormatterTest, FormatDouble) {
    value_formatter fmt(database_types::postgres);
    double value = 3.14159;
    std::string formatted = fmt.format(value);
    EXPECT_TRUE(formatted.find("3.14") != std::string::npos);
}

TEST_F(ValueFormatterTest, FormatBoolean) {
    value_formatter fmt(database_types::postgres);
    bool value = true;
    std::string formatted = fmt.format(value);
    EXPECT_EQ(formatted, "TRUE");
}
