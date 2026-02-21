/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Unit tests for immutable_query_builder SQL generation and
 * immutable builder pattern.
 * Part of #367, sub-issue #377.
 */

#include <gtest/gtest.h>
#include <string>

#include "database/query/immutable_query_builder.h"

using namespace database;

//=============================================================================
// immutable_query_builder Tests
//=============================================================================

class ImmutableQueryBuilderTest : public ::testing::Test {
protected:
	// Default builder on "users" table
	immutable_query_builder builder_{"users"};
};

// -- Construction --

TEST_F(ImmutableQueryBuilderTest, DefaultBuildSelectsStar) {
	std::string sql = builder_.build();
	EXPECT_NE(sql.find("SELECT *"), std::string::npos);
	EXPECT_NE(sql.find("\"users\""), std::string::npos);
}

// -- select() --

TEST_F(ImmutableQueryBuilderTest, SelectSpecificColumns) {
	auto q = builder_.select({"id", "name", "email"});
	std::string sql = q.build();
	EXPECT_NE(sql.find("\"id\""), std::string::npos);
	EXPECT_NE(sql.find("\"name\""), std::string::npos);
	EXPECT_NE(sql.find("\"email\""), std::string::npos);
	EXPECT_EQ(sql.find("*"), std::string::npos);
}

// -- where() --

TEST_F(ImmutableQueryBuilderTest, WhereWithFieldOpValue) {
	auto q = builder_.where("active", "=", true);
	std::string sql = q.build();
	EXPECT_NE(sql.find("WHERE"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, WhereMultipleConditionsJoinedByAnd) {
	auto q = builder_
		.where("active", "=", true)
		.where("age", ">", int64_t{18});
	std::string sql = q.build();
	EXPECT_NE(sql.find("AND"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, WhereWithQueryCondition) {
	query_condition cond("status = 'active'");
	auto q = builder_.where(cond);
	std::string sql = q.build();
	EXPECT_NE(sql.find("WHERE"), std::string::npos);
	EXPECT_NE(sql.find("status = 'active'"), std::string::npos);
}

// -- order_by() --

TEST_F(ImmutableQueryBuilderTest, OrderByAscending) {
	auto q = builder_.order_by("name", sort_order::asc);
	std::string sql = q.build();
	EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
	EXPECT_NE(sql.find("ASC"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, OrderByDescending) {
	auto q = builder_.order_by("created_at", sort_order::desc);
	std::string sql = q.build();
	EXPECT_NE(sql.find("DESC"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, OrderByMultipleFields) {
	auto q = builder_
		.order_by("name", sort_order::asc)
		.order_by("created_at", sort_order::desc);
	std::string sql = q.build();
	EXPECT_NE(sql.find("ASC"), std::string::npos);
	EXPECT_NE(sql.find("DESC"), std::string::npos);
}

// -- limit() and offset() --

TEST_F(ImmutableQueryBuilderTest, LimitClause) {
	auto q = builder_.limit(10);
	std::string sql = q.build();
	EXPECT_NE(sql.find("LIMIT 10"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, OffsetClause) {
	auto q = builder_.offset(20);
	std::string sql = q.build();
	EXPECT_NE(sql.find("OFFSET 20"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, LimitAndOffset) {
	auto q = builder_.limit(10).offset(20);
	std::string sql = q.build();
	EXPECT_NE(sql.find("LIMIT 10"), std::string::npos);
	EXPECT_NE(sql.find("OFFSET 20"), std::string::npos);
}

// -- join() --

TEST_F(ImmutableQueryBuilderTest, InnerJoin) {
	auto q = builder_.join("orders", "users.id = orders.user_id", join_type::inner);
	std::string sql = q.build();
	EXPECT_NE(sql.find("INNER JOIN"), std::string::npos);
	EXPECT_NE(sql.find("users.id = orders.user_id"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, LeftJoin) {
	auto q = builder_.join("orders", "users.id = orders.user_id", join_type::left);
	std::string sql = q.build();
	EXPECT_NE(sql.find("LEFT JOIN"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, RightJoin) {
	auto q = builder_.join("orders", "users.id = orders.user_id", join_type::right);
	std::string sql = q.build();
	EXPECT_NE(sql.find("RIGHT JOIN"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, FullOuterJoin) {
	auto q = builder_.join("orders", "users.id = orders.user_id", join_type::full_outer);
	std::string sql = q.build();
	EXPECT_NE(sql.find("FULL OUTER JOIN"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, CrossJoin) {
	auto q = builder_.join("roles", "1=1", join_type::cross);
	std::string sql = q.build();
	EXPECT_NE(sql.find("CROSS JOIN"), std::string::npos);
}

// -- group_by() and having() --

TEST_F(ImmutableQueryBuilderTest, GroupBy) {
	auto q = builder_.group_by({"department"});
	std::string sql = q.build();
	EXPECT_NE(sql.find("GROUP BY"), std::string::npos);
	EXPECT_NE(sql.find("\"department\""), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, GroupByMultipleFields) {
	auto q = builder_.group_by({"department", "status"});
	std::string sql = q.build();
	EXPECT_NE(sql.find("GROUP BY"), std::string::npos);
	EXPECT_NE(sql.find("\"department\""), std::string::npos);
	EXPECT_NE(sql.find("\"status\""), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, Having) {
	auto q = builder_.group_by({"department"}).having("COUNT(*) > 5");
	std::string sql = q.build();
	EXPECT_NE(sql.find("HAVING COUNT(*) > 5"), std::string::npos);
}

// -- build_for_database() --

TEST_F(ImmutableQueryBuilderTest, PostgresUsesDoubleQuotes) {
	auto q = builder_.select({"id", "name"});
	std::string sql = q.build_for_database(database_types::postgres);
	EXPECT_NE(sql.find("\"id\""), std::string::npos);
	EXPECT_NE(sql.find("\"name\""), std::string::npos);
	EXPECT_NE(sql.find("\"users\""), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, MySqlUsesBackticks) {
	auto q = builder_.select({"id", "name"});
	std::string sql = q.build_for_database(database_types::mysql);
	EXPECT_NE(sql.find("`id`"), std::string::npos);
	EXPECT_NE(sql.find("`name`"), std::string::npos);
	EXPECT_NE(sql.find("`users`"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, SqliteUsesDoubleQuotes) {
	auto q = builder_.select({"id"});
	std::string sql = q.build_for_database(database_types::sqlite);
	EXPECT_NE(sql.find("\"id\""), std::string::npos);
	EXPECT_NE(sql.find("\"users\""), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, DefaultBuildUsesPostgresDialect) {
	auto q = builder_.select({"id"});
	std::string default_sql = q.build();
	std::string pg_sql = q.build_for_database(database_types::postgres);
	EXPECT_EQ(default_sql, pg_sql);
}

// -- Immutability verification --

TEST_F(ImmutableQueryBuilderTest, OriginalUnchangedAfterSelect) {
	auto q = builder_.select({"id"});
	std::string original_sql = builder_.build();
	std::string new_sql = q.build();
	EXPECT_NE(original_sql, new_sql);
	EXPECT_NE(original_sql.find("*"), std::string::npos);
	EXPECT_EQ(new_sql.find("*"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, OriginalUnchangedAfterWhere) {
	auto q = builder_.where("id", "=", int64_t{1});
	std::string original_sql = builder_.build();
	EXPECT_EQ(original_sql.find("WHERE"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, OriginalUnchangedAfterLimit) {
	auto q = builder_.limit(10);
	std::string original_sql = builder_.build();
	EXPECT_EQ(original_sql.find("LIMIT"), std::string::npos);
}

TEST_F(ImmutableQueryBuilderTest, ChainingProducesCorrectQuery) {
	auto q = builder_
		.select({"id", "name"})
		.where("active", "=", true)
		.order_by("name", sort_order::asc)
		.limit(10)
		.offset(0);

	std::string sql = q.build();
	EXPECT_NE(sql.find("SELECT"), std::string::npos);
	EXPECT_NE(sql.find("WHERE"), std::string::npos);
	EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
	EXPECT_NE(sql.find("LIMIT 10"), std::string::npos);
	EXPECT_NE(sql.find("OFFSET 0"), std::string::npos);
}

// -- Full query composition --

TEST_F(ImmutableQueryBuilderTest, ComplexQueryComposition) {
	auto q = builder_
		.select({"u.id", "u.name", "COUNT(o.id)"})
		.join("orders", "u.id = o.user_id", join_type::left)
		.where("u.active", "=", true)
		.group_by({"u.id", "u.name"})
		.having("COUNT(o.id) > 0")
		.order_by("u.name", sort_order::asc)
		.limit(50);

	std::string sql = q.build();
	EXPECT_NE(sql.find("SELECT"), std::string::npos);
	EXPECT_NE(sql.find("LEFT JOIN"), std::string::npos);
	EXPECT_NE(sql.find("WHERE"), std::string::npos);
	EXPECT_NE(sql.find("GROUP BY"), std::string::npos);
	EXPECT_NE(sql.find("HAVING"), std::string::npos);
	EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
	EXPECT_NE(sql.find("LIMIT 50"), std::string::npos);
}
