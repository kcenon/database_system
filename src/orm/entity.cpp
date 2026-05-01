// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/orm/entity.h>
#include <sstream>
#include <algorithm>
#include <iostream>

// Logging helper macros - using std::cerr/cout for consistent behavior
// Note: For structured logging, use integrated_database module which provides
// logger_adapter. The database module itself should not depend on integrated_database
// to avoid circular dependencies.
#define ORM_LOG_ERROR(context, message) \
	std::cerr << "[ORM:" << context << "] Error: " << message << std::endl
#define ORM_LOG_WARNING(message) \
	std::cerr << "[ORM] Warning: " << message << std::endl
#define ORM_LOG_INFO(message) \
	std::cout << "[ORM] Info: " << message << std::endl

namespace database::orm
{
	// field_metadata implementation
	field_metadata::field_metadata(const std::string& name,
	                               const std::string& type_name,
	                               field_constraint constraints,
	                               const std::string& index_name,
	                               const std::string& foreign_table,
	                               const std::string& foreign_field)
		: name_(name)
		, type_name_(type_name)
		, constraints_(constraints)
		, index_name_(index_name)
		, foreign_table_(foreign_table)
		, foreign_field_(foreign_field)
	{
	}

	std::string field_metadata::to_sql_definition() const
	{
		std::ostringstream oss;
		oss << name_ << " ";

		// Map C++ types to SQL types
		if (type_name_ == "int32_t" || type_name_ == "int") {
			oss << "INTEGER";
		} else if (type_name_ == "int64_t") {
			oss << "BIGINT";
		} else if (type_name_ == "double") {
			oss << "DOUBLE PRECISION";
		} else if (type_name_ == "std::string") {
			oss << "VARCHAR(255)";
		} else if (type_name_ == "bool") {
			oss << "BOOLEAN";
		} else if (type_name_.find("time_point") != std::string::npos) {
			oss << "TIMESTAMP";
		} else {
			oss << "TEXT";
		}

		// Add constraints
		if (is_primary_key()) {
			oss << " PRIMARY KEY";
		}
		if (is_auto_increment()) {
			oss << " AUTO_INCREMENT";
		}
		if (is_not_null() && !is_primary_key()) {
			oss << " NOT NULL";
		}
		if (is_unique() && !is_primary_key()) {
			oss << " UNIQUE";
		}
		if (has_default_now()) {
			oss << " DEFAULT CURRENT_TIMESTAMP";
		}

		return oss.str();
	}

	// entity_metadata implementation
	entity_metadata::entity_metadata(const std::string& table_name)
		: table_name_(table_name)
	{
	}

	void entity_metadata::add_field(const field_metadata& field)
	{
		fields_.push_back(field);
	}

	const field_metadata* entity_metadata::get_primary_key() const
	{
		auto it = std::find_if(fields_.begin(), fields_.end(),
			[](const field_metadata& field) {
				return field.is_primary_key();
			});
		return (it != fields_.end()) ? &(*it) : nullptr;
	}

	std::vector<const field_metadata*> entity_metadata::get_indexes() const
	{
		std::vector<const field_metadata*> indexes;
		for (const auto& field : fields_) {
			if (field.has_index()) {
				indexes.push_back(&field);
			}
		}
		return indexes;
	}

	std::vector<const field_metadata*> entity_metadata::get_foreign_keys() const
	{
		std::vector<const field_metadata*> foreign_keys;
		for (const auto& field : fields_) {
			if (field.is_foreign_key()) {
				foreign_keys.push_back(&field);
			}
		}
		return foreign_keys;
	}

	std::string entity_metadata::create_table_sql() const
	{
		std::ostringstream oss;
		oss << "CREATE TABLE IF NOT EXISTS " << table_name_ << " (\n";

		bool first = true;
		for (const auto& field : fields_) {
			if (!first) oss << ",\n";
			oss << "  " << field.to_sql_definition();
			first = false;
		}

		// Add foreign key constraints
		auto foreign_keys = get_foreign_keys();
		for (const auto* fk : foreign_keys) {
			oss << ",\n  FOREIGN KEY (" << fk->name() << ") REFERENCES "
			    << fk->foreign_table() << "(" << fk->foreign_field() << ")";
		}

		oss << "\n)";
		return oss.str();
	}

	std::string entity_metadata::create_indexes_sql() const
	{
		std::ostringstream oss;
		auto indexes = get_indexes();

		for (const auto* index : indexes) {
			if (!index->index_name().empty()) {
				oss << "CREATE INDEX IF NOT EXISTS " << index->index_name()
				    << " ON " << table_name_ << "(" << index->name() << ");\n";
			} else {
				oss << "CREATE INDEX IF NOT EXISTS idx_" << table_name_
				    << "_" << index->name() << " ON " << table_name_
				    << "(" << index->name() << ");\n";
			}
		}

		return oss.str();
	}

	// entity_manager implementation
	bool entity_manager::create_tables(std::shared_ptr<core::database_backend> db)
	{
		if (!db) {
			ORM_LOG_ERROR("create_tables", "Database connection is null");
			return false;
		}

		try {
			for (const auto& [name, metadata] : metadata_cache_) {
				// Create table
				std::string create_sql = metadata->create_table_sql();
				auto result = db->execute_query(create_sql);
				if (result.is_err()) {
					ORM_LOG_ERROR("create_tables", "Failed to create table: " + name);
					return false;
				}

				// Create indexes
				std::string index_sql = metadata->create_indexes_sql();
				if (!index_sql.empty()) {
					auto index_result = db->execute_query(index_sql);
					if (index_result.is_err()) {
						ORM_LOG_ERROR("create_tables", "Failed to create indexes for table: " + name);
						return false;
					}
				}
			}
			return true;
		} catch (const std::exception& e) {
			ORM_LOG_ERROR("create_tables", std::string("Exception during table creation: ") + e.what());
			return false;
		}
	}

	bool entity_manager::drop_tables(std::shared_ptr<core::database_backend> db)
	{
		if (!db) {
			ORM_LOG_ERROR("drop_tables", "Database connection is null");
			return false;
		}

		try {
			for (const auto& [name, metadata] : metadata_cache_) {
				std::string drop_sql = "DROP TABLE IF EXISTS " + metadata->table_name();
				auto result = db->execute_query(drop_sql);
				if (result.is_err()) {
					ORM_LOG_ERROR("drop_tables", "Failed to drop table: " + name);
					return false;
				}
			}
			return true;
		} catch (const std::exception& e) {
			ORM_LOG_ERROR("drop_tables", std::string("Exception during table dropping: ") + e.what());
			return false;
		}
	}

	bool entity_manager::sync_schema(std::shared_ptr<core::database_backend> db)
	{
		if (!db) {
			ORM_LOG_ERROR("sync_schema", "Database connection is null");
			return false;
		}

		try {
			// For now, just recreate all tables
			// In a real implementation, this would do schema diffing
			if (!drop_tables(db)) {
				ORM_LOG_ERROR("sync_schema", "Failed to drop existing tables");
				return false;
			}

			if (!create_tables(db)) {
				ORM_LOG_ERROR("sync_schema", "Failed to create new tables");
				return false;
			}

			return true;
		} catch (const std::exception& e) {
			ORM_LOG_ERROR("sync_schema", std::string("Exception during schema sync: ") + e.what());
			return false;
		}
	}

	// Note: Template implementations moved to header file to avoid
	// template instantiation issues. Only non-template methods implemented here.

} // namespace database::orm