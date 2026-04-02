// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "database_types.h"
#include "core/database_backend.h"
#include "query_dialect.h"
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <initializer_list>

namespace database
{
	/**
	 * @enum join_type
	 * @brief Types of SQL joins.
	 */
	enum class join_type
	{
		inner,
		left,
		right,
		full_outer,
		cross
	};

	/**
	 * @enum sort_order
	 * @brief Sort order for ORDER BY clauses.
	 */
	enum class sort_order
	{
		asc,
		desc
	};

	/**
	 * @class query_condition
	 * @brief Represents a WHERE condition in a query.
	 */
	class query_condition
	{
	public:
		query_condition(const std::string& field, const std::string& op, const core::database_value& value);

		std::string to_sql() const;
		std::string to_mongodb() const;
		std::string to_redis() const;

		// Logical operators
		query_condition operator&&(const query_condition& other) const;
		query_condition operator||(const query_condition& other) const;

	private:
		// Private default constructor for internal use by logical operators
		query_condition();

		std::string field_;
		std::string operator_;
		core::database_value value_;
		std::string raw_condition_;
		std::vector<query_condition> sub_conditions_;
		std::string logical_operator_;
	};

	/**
	 * @class query_builder
	 * @brief Universal query builder that adapts to different database types.
	 *
	 * This class provides a unified interface for building queries across different
	 * database backends (PostgreSQL, SQLite, MongoDB, Redis) using the
	 * Strategy pattern.
	 *
	 * ### Thread Safety
	 * - NOT thread-safe. Each thread should use its own instance.
	 * - Create separate builders for each thread or protect with external mutex.
	 *
	 * ### Memory Efficiency
	 * - Only allocates ONE dialect instance per builder.
	 * - Dialect is lazily initialized when first needed.
	 *
	 * ### Example Usage
	 * ```cpp
	 * // SQL query
	 * query_builder builder(database_types::postgres);
	 * auto query = builder
	 *     .select({"id", "name"})
	 *     .from("users")
	 *     .where("active", "=", true)
	 *     .order_by("name")
	 *     .limit(10)
	 *     .build();
	 *
	 * // Switch database type
	 * builder.for_database(database_types::sqlite);
	 * auto sqlite_query = builder.select({"*"}).from("users").build();
	 * ```
	 */
	class query_builder
	{
	public:
		explicit query_builder(database_types db_type = database_types::none);
		~query_builder() = default;

		// Move-only (dialect ownership)
		query_builder(query_builder&&) noexcept = default;
		query_builder& operator=(query_builder&&) noexcept = default;
		query_builder(const query_builder&) = delete;
		query_builder& operator=(const query_builder&) = delete;

		// Set database type
		query_builder& for_database(database_types db_type);

		// SQL-style interface (works for PostgreSQL, SQLite)
		query_builder& select(const std::vector<std::string>& columns);
		query_builder& from(const std::string& table);
		query_builder& where(const std::string& field, const std::string& op, const core::database_value& value);
		query_builder& where(const query_condition& condition);
		query_builder& join(const std::string& table, const std::string& condition, join_type type = join_type::inner);
		query_builder& order_by(const std::string& column, sort_order order = sort_order::asc);
		query_builder& group_by(const std::vector<std::string>& columns);
		query_builder& group_by(const std::string& column);
		query_builder& having(const std::string& condition);
		query_builder& limit(size_t count);
		query_builder& offset(size_t count);

		// INSERT operations
		query_builder& insert_into(const std::string& table);
		query_builder& values(const std::map<std::string, core::database_value>& data);
		query_builder& values(const std::vector<std::map<std::string, core::database_value>>& rows);

		// UPDATE operations
		query_builder& update(const std::string& table);
		query_builder& set(const std::string& field, const core::database_value& value);
		query_builder& set(const std::map<std::string, core::database_value>& data);

		// DELETE operations
		query_builder& delete_from(const std::string& table);

		// NoSQL-style interface
		query_builder& collection(const std::string& name); // MongoDB
		query_builder& key(const std::string& key); // Redis

		// Build and execute
		std::string build() const;
		core::database_result execute(core::database_backend* db) const;

		// Reset builder
		void reset();

		// Get current database type
		database_types get_database_type() const;

	private:
		database_types db_type_;
		std::unique_ptr<query_dialect> dialect_;

		void ensure_dialect();
	};

} // namespace database
