// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file sql_query_builder_test.cpp
 * @brief Unit tests for SQL Query Builder using unified query_builder (DB-002)
 */

#include <gtest/gtest.h>
#include <kcenon/database/query_builder.h>
#include <string>
#include <map>
#include <vector>

namespace kcenon::database::tests
{

class SQLQueryBuilderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Use unified query_builder with PostgreSQL dialect (default for SQL)
        builder_ = std::make_unique<query_builder>(database_types::postgres);
    }

    void TearDown() override
    {
        builder_.reset();
    }

    std::unique_ptr<query_builder> builder_;
};

//=============================================================================
// SELECT Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, SimpleSelect)
{
    std::vector<std::string> cols = {"id", "name", "email"};
    auto query = builder_->select(cols)
                         .from("users")
                         .build();

    EXPECT_TRUE(query.find("SELECT") != std::string::npos);
    EXPECT_TRUE(query.find("FROM") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, SelectAll)
{
    auto query = builder_->select({"*"}).from("users").build();
    EXPECT_TRUE(query.find("SELECT") != std::string::npos);
    EXPECT_TRUE(query.find("*") != std::string::npos);
}

//=============================================================================
// JOIN Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, InnerJoin)
{
    std::vector<std::string> cols = {"u.id", "u.name", "o.total"};
    auto query = builder_->select(cols)
                         .from("users u")
                         .join("orders o", "u.id = o.user_id", join_type::inner)
                         .build();

    EXPECT_TRUE(query.find("INNER JOIN") != std::string::npos);
    EXPECT_TRUE(query.find("ON u.id = o.user_id") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, LeftJoin)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .join("profiles", "users.id = profiles.user_id", join_type::left)
                         .build();

    EXPECT_TRUE(query.find("LEFT JOIN") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, RightJoin)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .join("orders", "users.id = orders.user_id", join_type::right)
                         .build();

    EXPECT_TRUE(query.find("RIGHT JOIN") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, FullOuterJoin)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .join("orders", "users.id = orders.user_id", join_type::full_outer)
                         .build();

    EXPECT_TRUE(query.find("FULL OUTER JOIN") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, CrossJoin)
{
    auto query = builder_->select({"*"})
                         .from("colors")
                         .join("sizes", "1=1", join_type::cross)
                         .build();

    EXPECT_TRUE(query.find("CROSS JOIN") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, MultipleJoins)
{
    std::vector<std::string> cols = {"u.id", "u.name", "o.total", "p.name"};
    auto query = builder_->select(cols)
                         .from("users u")
                         .join("orders o", "u.id = o.user_id")
                         .join("products p", "o.product_id = p.id", join_type::left)
                         .build();

    EXPECT_TRUE(query.find("INNER JOIN") != std::string::npos);
    EXPECT_TRUE(query.find("LEFT JOIN") != std::string::npos);
}

//=============================================================================
// WHERE Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, SimpleWhere)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .where("active", "=", core::database_value{true})
                         .build();

    EXPECT_TRUE(query.find("WHERE") != std::string::npos);
    EXPECT_TRUE(query.find("active") != std::string::npos);
    EXPECT_TRUE(query.find("TRUE") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, WhereWithString)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .where("status", "=", core::database_value{std::string("active")})
                         .build();

    EXPECT_TRUE(query.find("'active'") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, WhereWithInt)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .where("age", ">", core::database_value{int64_t(18)})
                         .build();

    EXPECT_TRUE(query.find("> 18") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, WhereWithDouble)
{
    auto query = builder_->select({"*"})
                         .from("products")
                         .where("price", "<", core::database_value{99.99})
                         .build();

    EXPECT_TRUE(query.find("price") != std::string::npos);
    EXPECT_TRUE(query.find("<") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, MultipleWhereConditions)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .where("active", "=", core::database_value{true})
                         .where("age", ">", core::database_value{int64_t(18)})
                         .build();

    EXPECT_TRUE(query.find("AND") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, NestedConditionsWithAnd)
{
    query_condition cond1("age", ">", core::database_value{int64_t(18)});
    query_condition cond2("status", "=", core::database_value{std::string("active")});
    auto combined = cond1 && cond2;

    auto query = builder_->select({"*"})
                         .from("users")
                         .where(combined)
                         .build();

    EXPECT_TRUE(query.find("AND") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, NestedConditionsWithOr)
{
    query_condition cond1("role", "=", core::database_value{std::string("admin")});
    query_condition cond2("role", "=", core::database_value{std::string("superadmin")});
    auto combined = cond1 || cond2;

    auto query = builder_->select({"*"})
                         .from("users")
                         .where(combined)
                         .build();

    EXPECT_TRUE(query.find("OR") != std::string::npos);
}

//=============================================================================
// GROUP BY & HAVING Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, GroupBy)
{
    std::vector<std::string> cols = {"department", "COUNT(*)"};
    auto query = builder_->select(cols)
                         .from("employees")
                         .group_by("department")
                         .build();

    EXPECT_TRUE(query.find("GROUP BY") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, GroupByMultipleColumns)
{
    std::vector<std::string> cols = {"department", "city", "COUNT(*)"};
    std::vector<std::string> group_cols = {"department", "city"};
    auto query = builder_->select(cols)
                         .from("employees")
                         .group_by(group_cols)
                         .build();

    EXPECT_TRUE(query.find("GROUP BY") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, GroupByWithHaving)
{
    std::vector<std::string> cols = {"department", "COUNT(*) as count"};
    auto query = builder_->select(cols)
                         .from("employees")
                         .group_by("department")
                         .having("COUNT(*) > 5")
                         .build();

    EXPECT_TRUE(query.find("GROUP BY") != std::string::npos);
    EXPECT_TRUE(query.find("HAVING") != std::string::npos);
    EXPECT_TRUE(query.find("COUNT(*) > 5") != std::string::npos);
}

//=============================================================================
// ORDER BY Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, OrderByAsc)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .order_by("name", sort_order::asc)
                         .build();

    EXPECT_TRUE(query.find("ORDER BY") != std::string::npos);
    EXPECT_TRUE(query.find("name ASC") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, OrderByDesc)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .order_by("created_at", sort_order::desc)
                         .build();

    EXPECT_TRUE(query.find("ORDER BY") != std::string::npos);
    EXPECT_TRUE(query.find("created_at DESC") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, OrderByMultipleColumns)
{
    auto query = builder_->select({"*"})
                         .from("products")
                         .order_by("category", sort_order::asc)
                         .order_by("price", sort_order::desc)
                         .build();

    EXPECT_TRUE(query.find("ORDER BY") != std::string::npos);
    EXPECT_TRUE(query.find("category ASC") != std::string::npos);
    EXPECT_TRUE(query.find("price DESC") != std::string::npos);
}

//=============================================================================
// LIMIT & OFFSET Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, Limit)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .limit(10)
                         .build();

    EXPECT_TRUE(query.find("LIMIT 10") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, LimitWithOffset)
{
    auto query = builder_->select({"*"})
                         .from("users")
                         .limit(10)
                         .offset(20)
                         .build();

    EXPECT_TRUE(query.find("LIMIT 10") != std::string::npos);
    EXPECT_TRUE(query.find("OFFSET 20") != std::string::npos);
}

//=============================================================================
// INSERT Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, InsertSingleRow)
{
    std::map<std::string, core::database_value> data;
    data["name"] = core::database_value{std::string("John")};
    data["email"] = core::database_value{std::string("john@example.com")};

    auto query = builder_->insert_into("users")
                         .values(data)
                         .build();

    EXPECT_TRUE(query.find("INSERT INTO") != std::string::npos);
    EXPECT_TRUE(query.find("VALUES") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, InsertMultipleRows)
{
    std::vector<std::map<std::string, core::database_value>> rows;

    std::map<std::string, core::database_value> row1;
    row1["name"] = core::database_value{std::string("John")};
    row1["age"] = core::database_value{int64_t(30)};
    rows.push_back(row1);

    std::map<std::string, core::database_value> row2;
    row2["name"] = core::database_value{std::string("Jane")};
    row2["age"] = core::database_value{int64_t(25)};
    rows.push_back(row2);

    auto query = builder_->insert_into("users")
                         .values(rows)
                         .build();

    EXPECT_TRUE(query.find("INSERT INTO") != std::string::npos);
    EXPECT_TRUE(query.find("VALUES") != std::string::npos);
}

//=============================================================================
// UPDATE Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, UpdateSingleField)
{
    auto query = builder_->update("users")
                         .set("status", core::database_value{std::string("active")})
                         .where("id", "=", core::database_value{int64_t(1)})
                         .build();

    EXPECT_TRUE(query.find("UPDATE") != std::string::npos);
    EXPECT_TRUE(query.find("SET") != std::string::npos);
    EXPECT_TRUE(query.find("WHERE") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, UpdateMultipleFields)
{
    std::map<std::string, core::database_value> data;
    data["status"] = core::database_value{std::string("active")};
    data["updated_at"] = core::database_value{std::string("2025-01-01")};

    auto query = builder_->update("users")
                         .set(data)
                         .where("id", "=", core::database_value{int64_t(1)})
                         .build();

    EXPECT_TRUE(query.find("UPDATE") != std::string::npos);
    EXPECT_TRUE(query.find("SET") != std::string::npos);
}

//=============================================================================
// DELETE Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, DeleteWithWhere)
{
    auto query = builder_->delete_from("users")
                         .where("id", "=", core::database_value{int64_t(1)})
                         .build();

    EXPECT_TRUE(query.find("DELETE FROM") != std::string::npos);
    EXPECT_TRUE(query.find("WHERE") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, DeleteWithMultipleConditions)
{
    auto query = builder_->delete_from("users")
                         .where("status", "=", core::database_value{std::string("inactive")})
                         .where("last_login", "<", core::database_value{std::string("2024-01-01")})
                         .build();

    EXPECT_TRUE(query.find("DELETE FROM") != std::string::npos);
    EXPECT_TRUE(query.find("AND") != std::string::npos);
}

//=============================================================================
// Database-Specific Syntax Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, PostgreSQLSyntax)
{
    query_builder pg_builder(database_types::postgres);
    auto query = pg_builder.select({"*"})
                          .from("users")
                          .limit(10)
                          .build();

    EXPECT_TRUE(query.find("\"users\"") != std::string::npos);
}

TEST_F(SQLQueryBuilderTest, SQLiteSyntax)
{
    query_builder sqlite_builder(database_types::sqlite);
    auto query = sqlite_builder.select({"*"})
                              .from("users")
                              .limit(10)
                              .build();

    EXPECT_TRUE(query.find("[users]") != std::string::npos);
}

//=============================================================================
// Reset & Reuse Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, Reset)
{
    builder_->select({"*"}).from("users").limit(10);
    builder_->reset();

    // After reset, builder should be clean - verify by building a new query
    // Reset clears the state, so next query starts fresh
    auto query = builder_->select({"id"}).from("test_table").build();
    EXPECT_TRUE(query.find("test_table") != std::string::npos);
    EXPECT_TRUE(query.find("users") == std::string::npos);  // Previous table not present
}

TEST_F(SQLQueryBuilderTest, ReuseAfterReset)
{
    builder_->select({"*"}).from("users").limit(10);
    builder_->reset();

    auto query = builder_->select({"*"}).from("products").build();
    EXPECT_TRUE(query.find("products") != std::string::npos);
    EXPECT_TRUE(query.find("users") == std::string::npos);
}

//=============================================================================
// Complex Query Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, ComplexSelectQuery)
{
    std::vector<std::string> cols = {"u.id", "u.name", "COUNT(o.id) as order_count"};
    std::vector<std::string> group_cols = {"u.id", "u.name"};
    auto query = builder_->select(cols)
                         .from("users u")
                         .join("orders o", "u.id = o.user_id", join_type::left)
                         .where("u.status", "=", core::database_value{std::string("active")})
                         .group_by(group_cols)
                         .having("COUNT(o.id) > 5")
                         .order_by("order_count", sort_order::desc)
                         .limit(10)
                         .build();

    EXPECT_TRUE(query.find("SELECT") != std::string::npos);
    EXPECT_TRUE(query.find("LEFT JOIN") != std::string::npos);
    EXPECT_TRUE(query.find("WHERE") != std::string::npos);
    EXPECT_TRUE(query.find("GROUP BY") != std::string::npos);
    EXPECT_TRUE(query.find("HAVING") != std::string::npos);
    EXPECT_TRUE(query.find("ORDER BY") != std::string::npos);
    EXPECT_TRUE(query.find("LIMIT") != std::string::npos);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(SQLQueryBuilderTest, EmptyConditions)
{
    auto query = builder_->select({"*"}).from("users").build();

    EXPECT_FALSE(query.find("WHERE") != std::string::npos);
}

//=============================================================================
// For Database Switch Tests
//=============================================================================

TEST_F(SQLQueryBuilderTest, SwitchDatabase)
{
    builder_->for_database(database_types::sqlite);

    auto query = builder_->select({"*"})
                        .from("users")
                        .build();

    EXPECT_TRUE(query.find("[users]") != std::string::npos);  // SQLite syntax
}

} // namespace kcenon::database::tests
