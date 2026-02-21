/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Unit tests for ORM entity metadata: field_metadata, entity_metadata.
 * Part of #367, sub-issue #377.
 */

#include <gtest/gtest.h>
#include <string>

#include "database/orm/entity.h"

using namespace database::orm;

//=============================================================================
// field_metadata Tests
//=============================================================================

class FieldMetadataTest : public ::testing::Test {};

// -- Constructor and accessors --

TEST_F(FieldMetadataTest, ConstructsWithNameAndType) {
	field_metadata field("id", "int64_t");
	EXPECT_EQ(field.name(), "id");
	EXPECT_EQ(field.type_name(), "int64_t");
}

TEST_F(FieldMetadataTest, DefaultConstraintIsNone) {
	field_metadata field("name", "std::string");
	EXPECT_FALSE(field.is_primary_key());
	EXPECT_FALSE(field.is_not_null());
	EXPECT_FALSE(field.is_unique());
	EXPECT_FALSE(field.is_auto_increment());
	EXPECT_FALSE(field.has_index());
	EXPECT_FALSE(field.is_foreign_key());
	EXPECT_FALSE(field.has_default_now());
}

TEST_F(FieldMetadataTest, PrimaryKeyConstraint) {
	field_metadata field("id", "int64_t", field_constraint::primary_key);
	EXPECT_TRUE(field.is_primary_key());
	EXPECT_FALSE(field.is_not_null());
}

TEST_F(FieldMetadataTest, CombinedConstraints) {
	field_metadata field("id", "int64_t",
		field_constraint::primary_key | field_constraint::auto_increment);
	EXPECT_TRUE(field.is_primary_key());
	EXPECT_TRUE(field.is_auto_increment());
	EXPECT_FALSE(field.is_not_null());
}

TEST_F(FieldMetadataTest, NotNullAndUniqueConstraints) {
	field_metadata field("email", "std::string",
		field_constraint::not_null | field_constraint::unique);
	EXPECT_TRUE(field.is_not_null());
	EXPECT_TRUE(field.is_unique());
	EXPECT_FALSE(field.is_primary_key());
}

TEST_F(FieldMetadataTest, IndexConstraintWithName) {
	field_metadata field("name", "std::string",
		field_constraint::index, "idx_users_name");
	EXPECT_TRUE(field.has_index());
	EXPECT_EQ(field.index_name(), "idx_users_name");
}

TEST_F(FieldMetadataTest, ForeignKeyConstraint) {
	field_metadata field("user_id", "int64_t",
		field_constraint::foreign_key, "", "users", "id");
	EXPECT_TRUE(field.is_foreign_key());
	EXPECT_EQ(field.foreign_table(), "users");
	EXPECT_EQ(field.foreign_field(), "id");
}

TEST_F(FieldMetadataTest, DefaultNowConstraint) {
	field_metadata field("created_at", "std::chrono::system_clock::time_point",
		field_constraint::default_now);
	EXPECT_TRUE(field.has_default_now());
}

// -- to_sql_definition() --

TEST_F(FieldMetadataTest, SqlDefinitionInt32) {
	field_metadata field("count", "int32_t");
	EXPECT_EQ(field.to_sql_definition(), "count INTEGER");
}

TEST_F(FieldMetadataTest, SqlDefinitionInt64) {
	field_metadata field("id", "int64_t");
	EXPECT_EQ(field.to_sql_definition(), "id BIGINT");
}

TEST_F(FieldMetadataTest, SqlDefinitionDouble) {
	field_metadata field("price", "double");
	EXPECT_EQ(field.to_sql_definition(), "price DOUBLE PRECISION");
}

TEST_F(FieldMetadataTest, SqlDefinitionString) {
	field_metadata field("name", "std::string");
	EXPECT_EQ(field.to_sql_definition(), "name VARCHAR(255)");
}

TEST_F(FieldMetadataTest, SqlDefinitionBool) {
	field_metadata field("active", "bool");
	EXPECT_EQ(field.to_sql_definition(), "active BOOLEAN");
}

TEST_F(FieldMetadataTest, SqlDefinitionTimestamp) {
	field_metadata field("created_at", "std::chrono::system_clock::time_point");
	EXPECT_EQ(field.to_sql_definition(), "created_at TIMESTAMP");
}

TEST_F(FieldMetadataTest, SqlDefinitionUnknownTypeFallsBackToText) {
	field_metadata field("data", "custom_type");
	EXPECT_EQ(field.to_sql_definition(), "data TEXT");
}

TEST_F(FieldMetadataTest, SqlDefinitionPrimaryKey) {
	field_metadata field("id", "int64_t", field_constraint::primary_key);
	std::string sql = field.to_sql_definition();
	EXPECT_NE(sql.find("PRIMARY KEY"), std::string::npos);
}

TEST_F(FieldMetadataTest, SqlDefinitionAutoIncrement) {
	field_metadata field("id", "int64_t",
		field_constraint::primary_key | field_constraint::auto_increment);
	std::string sql = field.to_sql_definition();
	EXPECT_NE(sql.find("PRIMARY KEY"), std::string::npos);
	EXPECT_NE(sql.find("AUTO_INCREMENT"), std::string::npos);
}

TEST_F(FieldMetadataTest, SqlDefinitionNotNull) {
	field_metadata field("name", "std::string", field_constraint::not_null);
	std::string sql = field.to_sql_definition();
	EXPECT_NE(sql.find("NOT NULL"), std::string::npos);
}

TEST_F(FieldMetadataTest, SqlDefinitionUniqueNonPK) {
	field_metadata field("email", "std::string", field_constraint::unique);
	std::string sql = field.to_sql_definition();
	EXPECT_NE(sql.find("UNIQUE"), std::string::npos);
}

TEST_F(FieldMetadataTest, SqlDefinitionPrimaryKeyOmitsNotNull) {
	// Primary keys are implicitly NOT NULL, so NOT NULL should not appear
	field_metadata field("id", "int64_t",
		field_constraint::primary_key | field_constraint::not_null);
	std::string sql = field.to_sql_definition();
	EXPECT_NE(sql.find("PRIMARY KEY"), std::string::npos);
	EXPECT_EQ(sql.find("NOT NULL"), std::string::npos);
}

TEST_F(FieldMetadataTest, SqlDefinitionPrimaryKeyOmitsUnique) {
	// Primary keys are implicitly UNIQUE, so UNIQUE should not appear
	field_metadata field("id", "int64_t",
		field_constraint::primary_key | field_constraint::unique);
	std::string sql = field.to_sql_definition();
	EXPECT_NE(sql.find("PRIMARY KEY"), std::string::npos);
	EXPECT_EQ(sql.find("UNIQUE"), std::string::npos);
}

TEST_F(FieldMetadataTest, SqlDefinitionDefaultNow) {
	field_metadata field("created_at", "std::chrono::system_clock::time_point",
		field_constraint::default_now);
	std::string sql = field.to_sql_definition();
	EXPECT_NE(sql.find("DEFAULT CURRENT_TIMESTAMP"), std::string::npos);
}

// -- has_constraint helper function --

TEST_F(FieldMetadataTest, HasConstraintFunction) {
	EXPECT_TRUE(has_constraint(
		field_constraint::primary_key | field_constraint::not_null,
		field_constraint::primary_key));
	EXPECT_TRUE(has_constraint(
		field_constraint::primary_key | field_constraint::not_null,
		field_constraint::not_null));
	EXPECT_FALSE(has_constraint(
		field_constraint::primary_key | field_constraint::not_null,
		field_constraint::unique));
}

//=============================================================================
// entity_metadata Tests
//=============================================================================

class EntityMetadataTest : public ::testing::Test {
protected:
	void SetUp() override {
		meta_ = std::make_unique<entity_metadata>("users");

		meta_->add_field(field_metadata("id", "int64_t",
			field_constraint::primary_key | field_constraint::auto_increment));
		meta_->add_field(field_metadata("name", "std::string",
			field_constraint::not_null));
		meta_->add_field(field_metadata("email", "std::string",
			field_constraint::not_null | field_constraint::unique | field_constraint::index,
			"idx_users_email"));
		meta_->add_field(field_metadata("department_id", "int64_t",
			field_constraint::foreign_key, "", "departments", "id"));
		meta_->add_field(field_metadata("created_at", "std::chrono::system_clock::time_point",
			field_constraint::default_now));
	}

	std::unique_ptr<entity_metadata> meta_;
};

TEST_F(EntityMetadataTest, TableName) {
	EXPECT_EQ(meta_->table_name(), "users");
}

TEST_F(EntityMetadataTest, FieldCount) {
	EXPECT_EQ(meta_->fields().size(), 5u);
}

TEST_F(EntityMetadataTest, AddFieldStoresInOrder) {
	EXPECT_EQ(meta_->fields()[0].name(), "id");
	EXPECT_EQ(meta_->fields()[1].name(), "name");
	EXPECT_EQ(meta_->fields()[2].name(), "email");
	EXPECT_EQ(meta_->fields()[3].name(), "department_id");
	EXPECT_EQ(meta_->fields()[4].name(), "created_at");
}

TEST_F(EntityMetadataTest, GetPrimaryKeyFindsId) {
	const auto* pk = meta_->get_primary_key();
	ASSERT_NE(pk, nullptr);
	EXPECT_EQ(pk->name(), "id");
	EXPECT_TRUE(pk->is_primary_key());
}

TEST_F(EntityMetadataTest, GetPrimaryKeyReturnsNullWhenMissing) {
	entity_metadata meta("no_pk_table");
	meta.add_field(field_metadata("name", "std::string"));
	EXPECT_EQ(meta.get_primary_key(), nullptr);
}

TEST_F(EntityMetadataTest, GetIndexesFindsIndexedFields) {
	auto indexes = meta_->get_indexes();
	ASSERT_EQ(indexes.size(), 1u);
	EXPECT_EQ(indexes[0]->name(), "email");
}

TEST_F(EntityMetadataTest, GetForeignKeysFindsFK) {
	auto fks = meta_->get_foreign_keys();
	ASSERT_EQ(fks.size(), 1u);
	EXPECT_EQ(fks[0]->name(), "department_id");
	EXPECT_EQ(fks[0]->foreign_table(), "departments");
	EXPECT_EQ(fks[0]->foreign_field(), "id");
}

TEST_F(EntityMetadataTest, CreateTableSqlContainsTableName) {
	std::string sql = meta_->create_table_sql();
	EXPECT_NE(sql.find("CREATE TABLE IF NOT EXISTS users"), std::string::npos);
}

TEST_F(EntityMetadataTest, CreateTableSqlContainsAllColumns) {
	std::string sql = meta_->create_table_sql();
	EXPECT_NE(sql.find("id BIGINT PRIMARY KEY AUTO_INCREMENT"), std::string::npos);
	EXPECT_NE(sql.find("name VARCHAR(255) NOT NULL"), std::string::npos);
	EXPECT_NE(sql.find("email VARCHAR(255) NOT NULL UNIQUE"), std::string::npos);
	EXPECT_NE(sql.find("department_id BIGINT"), std::string::npos);
	EXPECT_NE(sql.find("created_at TIMESTAMP"), std::string::npos);
}

TEST_F(EntityMetadataTest, CreateTableSqlContainsForeignKeyConstraint) {
	std::string sql = meta_->create_table_sql();
	EXPECT_NE(sql.find("FOREIGN KEY (department_id) REFERENCES departments(id)"),
		std::string::npos);
}

TEST_F(EntityMetadataTest, CreateIndexesSqlWithNamedIndex) {
	std::string sql = meta_->create_indexes_sql();
	EXPECT_NE(sql.find("CREATE INDEX IF NOT EXISTS idx_users_email ON users(email)"),
		std::string::npos);
}

TEST_F(EntityMetadataTest, CreateIndexesSqlAutoGeneratesNameWhenMissing) {
	entity_metadata meta("products");
	meta.add_field(field_metadata("category", "std::string", field_constraint::index));

	std::string sql = meta.create_indexes_sql();
	EXPECT_NE(sql.find("CREATE INDEX IF NOT EXISTS idx_products_category ON products(category)"),
		std::string::npos);
}

TEST_F(EntityMetadataTest, CreateIndexesSqlEmptyWhenNoIndexes) {
	entity_metadata meta("simple");
	meta.add_field(field_metadata("name", "std::string"));

	std::string sql = meta.create_indexes_sql();
	EXPECT_TRUE(sql.empty());
}

TEST_F(EntityMetadataTest, EmptyTableProducesMinimalDDL) {
	entity_metadata meta("empty_table");
	std::string sql = meta.create_table_sql();
	EXPECT_NE(sql.find("CREATE TABLE IF NOT EXISTS empty_table"), std::string::npos);
}

// -- Constraint helper functions --

TEST_F(EntityMetadataTest, ConstraintHelperFunctions) {
	EXPECT_EQ(static_cast<int>(primary_key()), static_cast<int>(field_constraint::primary_key));
	EXPECT_EQ(static_cast<int>(not_null()), static_cast<int>(field_constraint::not_null));
	EXPECT_EQ(static_cast<int>(unique()), static_cast<int>(field_constraint::unique));
	EXPECT_EQ(static_cast<int>(auto_increment()), static_cast<int>(field_constraint::auto_increment));
	EXPECT_EQ(static_cast<int>(default_now()), static_cast<int>(field_constraint::default_now));
}
