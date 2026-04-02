// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file orm_entity_demo.cpp
 * @brief Demonstrates the ORM entity system with field metadata and SQL
 *        generation.
 *
 * This example shows how to:
 * - Define an entity class deriving from entity_base
 * - Configure field constraints (primary key, not null, unique, index)
 * - Generate CREATE TABLE SQL from entity metadata
 * - Inspect entity field metadata programmatically
 * - Use type-safe field accessors
 * - Check type traits for entity and field types
 *
 * Note: This example focuses on the ORM metadata and schema generation
 * capabilities. It does not require a running database.
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "database/orm/entity.h"
#include "database/database_types.h"

using namespace database;
using namespace database::orm;

// -------------------------------------------------------
// Define a User entity
//
// We manually implement table_name() and define fields
// using the ENTITY_FIELD macro for type-safe accessors.
// -------------------------------------------------------
class user_entity : public entity_base
{
public:
	using primary_key_type = int64_t;

	std::string table_name() const override { return "users"; }

	// Declare fields with constraints
	ENTITY_FIELD(int64_t, id,
				 field_constraint::primary_key | field_constraint::auto_increment)
	ENTITY_FIELD(std::string, username,
				 field_constraint::not_null | field_constraint::unique)
	ENTITY_FIELD(std::string, email,
				 field_constraint::not_null | field_constraint::unique)
	ENTITY_FIELD(int32_t, age, field_constraint::not_null)
	ENTITY_FIELD(bool, is_active, field_constraint::not_null)

	ENTITY_METADATA()

	// CRUD operations (stubs for this demo)
	bool save() override { return false; }
	bool load() override { return false; }
	bool update() override { return false; }
	bool remove() override { return false; }

private:
	static inline entity_metadata metadata_{"users"};
};

// Initialize entity metadata by registering each field
void user_entity::initialize_metadata()
{
	metadata_.add_field(id_metadata_);
	metadata_.add_field(username_metadata_);
	metadata_.add_field(email_metadata_);
	metadata_.add_field(age_metadata_);
	metadata_.add_field(is_active_metadata_);
}

// -------------------------------------------------------
// Define a Post entity with indexed field
// -------------------------------------------------------
class post_entity : public entity_base
{
public:
	using primary_key_type = int64_t;

	std::string table_name() const override { return "posts"; }

	ENTITY_FIELD(int64_t, id,
				 field_constraint::primary_key | field_constraint::auto_increment)
	ENTITY_FIELD(std::string, title, field_constraint::not_null)
	ENTITY_FIELD(std::string, body, field_constraint::none)
	ENTITY_FIELD(int64_t, author_id,
				 field_constraint::not_null | field_constraint::index)

	ENTITY_METADATA()

	bool save() override { return false; }
	bool load() override { return false; }
	bool update() override { return false; }
	bool remove() override { return false; }

private:
	static inline entity_metadata metadata_{"posts"};
};

void post_entity::initialize_metadata()
{
	metadata_.add_field(id_metadata_);
	metadata_.add_field(title_metadata_);
	metadata_.add_field(body_metadata_);
	metadata_.add_field(author_id_metadata_);
}

// -------------------------------------------------------
// Main
// -------------------------------------------------------
int main()
{
	std::cout << "=== orm_entity_demo example ===" << std::endl;

	// -------------------------------------------------------
	// 1. Inspect User entity metadata
	// -------------------------------------------------------
	std::cout << "\n--- User entity metadata ---" << std::endl;
	{
		user_entity user;
		const auto& meta = user.get_metadata();

		std::cout << "Table: " << meta.table_name() << std::endl;
		std::cout << "Fields:" << std::endl;

		for (const auto& field : meta.fields())
		{
			std::cout << "  " << field.name()
					  << " (" << field.type_name() << ")";

			if (field.is_primary_key())
			{
				std::cout << " [PK]";
			}
			if (field.is_auto_increment())
			{
				std::cout << " [AUTO]";
			}
			if (field.is_not_null())
			{
				std::cout << " [NOT NULL]";
			}
			if (field.is_unique())
			{
				std::cout << " [UNIQUE]";
			}
			if (field.has_index())
			{
				std::cout << " [INDEX]";
			}
			std::cout << std::endl;
		}

		// Primary key info
		const auto* pk = meta.get_primary_key();
		if (pk != nullptr)
		{
			std::cout << "Primary key: " << pk->name() << std::endl;
		}
	}

	// -------------------------------------------------------
	// 2. Generate CREATE TABLE SQL
	// -------------------------------------------------------
	std::cout << "\n--- Generated SQL ---" << std::endl;
	{
		user_entity user;
		std::cout << "User table SQL:" << std::endl;
		std::cout << user.get_metadata().create_table_sql() << std::endl;

		post_entity post;
		std::cout << "\nPost table SQL:" << std::endl;
		std::cout << post.get_metadata().create_table_sql() << std::endl;
	}

	// -------------------------------------------------------
	// 3. Generate index SQL
	// -------------------------------------------------------
	std::cout << "\n--- Generated index SQL ---" << std::endl;
	{
		post_entity post;
		auto index_sql = post.get_metadata().create_indexes_sql();
		if (!index_sql.empty())
		{
			std::cout << "Post indexes:" << std::endl;
			std::cout << index_sql << std::endl;
		}
		else
		{
			std::cout << "No additional indexes defined" << std::endl;
		}
	}

	// -------------------------------------------------------
	// 4. Using field accessors
	// -------------------------------------------------------
	std::cout << "\n--- Field accessors ---" << std::endl;
	{
		user_entity user;

		// Set field values through accessors
		user.id = int64_t{1};
		user.username = std::string("john_doe");
		user.email = std::string("john@example.com");
		user.age = int32_t{30};
		user.is_active = true;

		// Read field values
		std::cout << "User: "
				  << "id=" << user.id.get()
				  << " username=" << user.username.get()
				  << " email=" << user.email.get()
				  << " age=" << user.age.get()
				  << " active=" << std::boolalpha << user.is_active.get()
				  << std::endl;
	}

	// -------------------------------------------------------
	// 5. Type trait checks
	// -------------------------------------------------------
	std::cout << "\n--- Type trait checks ---" << std::endl;
	{
		std::cout << "is_entity<user_entity>: "
				  << std::boolalpha << is_entity_v<user_entity> << std::endl;
		std::cout << "is_entity<post_entity>: "
				  << std::boolalpha << is_entity_v<post_entity> << std::endl;
		std::cout << "is_entity<int>: "
				  << std::boolalpha << is_entity_v<int> << std::endl;

		std::cout << "is_field_type<int32_t>: "
				  << std::boolalpha << is_field_type_v<int32_t> << std::endl;
		std::cout << "is_field_type<std::string>: "
				  << std::boolalpha << is_field_type_v<std::string> << std::endl;
		std::cout << "is_field_type<std::vector<int>>: "
				  << std::boolalpha << is_field_type_v<std::vector<int>> << std::endl;
	}

	std::cout << "\n=== orm_entity_demo example completed ===" << std::endl;
	return 0;
}
