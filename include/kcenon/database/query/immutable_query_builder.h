// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/database_types.h>
#include <kcenon/database/query_builder.h>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace database
{
	/**
	 * @class immutable_query_builder
	 * @brief Thread-safe immutable query builder using functional programming style.
	 *
	 * ### Thread Safety
	 * - FULLY thread-safe. All methods are const and return new instances.
	 * - No mutable state. All fields are const.
	 * - Safe to share instances across threads.
	 * - Immutable builder pattern ensures no race conditions.
	 *
	 * ### Design
	 * - Each method returns a new instance with updated state.
	 * - Original instance remains unchanged.
	 * - Functional programming style (copy-on-write).
	 * - No locks needed due to immutability.
	 *
	 * ### Usage Example
	 * @code
	 * auto query = immutable_query_builder("users")
	 *     .select({"id", "name", "email"})
	 *     .where("active", "=", true)
	 *     .order_by("name", sort_order::asc)
	 *     .limit(10)
	 *     .build();
	 * @endcode
	 */
	class immutable_query_builder
	{
	public:
		/**
		 * @brief Constructs a new immutable query builder.
		 * @param table Table name for the query.
		 */
		explicit immutable_query_builder(const std::string& table);

		/**
		 * @brief SELECT clause - specifies which columns to select.
		 * @param fields Vector of column names to select.
		 * @return New immutable_query_builder instance with updated select fields.
		 */
		immutable_query_builder select(std::vector<std::string> fields) const;

		/**
		 * @brief WHERE clause - adds a condition.
		 * @param field Field name.
		 * @param op Operator (e.g., "=", ">", "<", "LIKE").
		 * @param value Value to compare against.
		 * @return New immutable_query_builder instance with updated conditions.
		 */
		immutable_query_builder where(const std::string& field, const std::string& op, const core::database_value& value) const;

		/**
		 * @brief WHERE clause - adds a query_condition.
		 * @param condition Pre-built query_condition.
		 * @return New immutable_query_builder instance with updated conditions.
		 */
		immutable_query_builder where(const query_condition& condition) const;

		/**
		 * @brief ORDER BY clause - specifies sorting.
		 * @param field Field name to sort by.
		 * @param order Sort order (asc or desc).
		 * @return New immutable_query_builder instance with updated order.
		 */
		immutable_query_builder order_by(const std::string& field, sort_order order = sort_order::asc) const;

		/**
		 * @brief LIMIT clause - limits number of results.
		 * @param count Maximum number of rows to return.
		 * @return New immutable_query_builder instance with updated limit.
		 */
		immutable_query_builder limit(uint32_t count) const;

		/**
		 * @brief OFFSET clause - skips rows.
		 * @param count Number of rows to skip.
		 * @return New immutable_query_builder instance with updated offset.
		 */
		immutable_query_builder offset(uint32_t count) const;

		/**
		 * @brief JOIN clause - adds a join.
		 * @param table Table to join.
		 * @param condition Join condition.
		 * @param type Join type (inner, left, right, etc.).
		 * @return New immutable_query_builder instance with updated join.
		 */
		immutable_query_builder join(const std::string& table, const std::string& condition, join_type type = join_type::inner) const;

		/**
		 * @brief GROUP BY clause - groups results.
		 * @param fields Fields to group by.
		 * @return New immutable_query_builder instance with updated grouping.
		 */
		immutable_query_builder group_by(std::vector<std::string> fields) const;

		/**
		 * @brief HAVING clause - filters grouped results.
		 * @param condition Condition for filtering groups.
		 * @return New immutable_query_builder instance with updated having clause.
		 */
		immutable_query_builder having(const std::string& condition) const;

		/**
		 * @brief Builds the final SQL query string.
		 * @return SQL query string.
		 *
		 * Thread-safe: No state modification, all fields are const.
		 */
		std::string build() const;

		/**
		 * @brief Builds the SQL query for a specific database type.
		 * @param db_type Database type (PostgreSQL, SQLite, etc.).
		 * @return SQL query string formatted for the specified database.
		 */
		std::string build_for_database(database_types db_type) const;

	private:
		// All fields are const - immutable state
		const std::string table_;
		const std::vector<std::string> select_fields_;
		const std::vector<query_condition> conditions_;
		const std::vector<std::pair<std::string, sort_order>> order_by_;
		const std::optional<uint32_t> limit_;
		const std::optional<uint32_t> offset_;
		const std::vector<std::tuple<std::string, std::string, join_type>> joins_;
		const std::vector<std::string> group_by_fields_;
		const std::string having_clause_;

		// Private constructor for internal use (copy with modification)
		immutable_query_builder(
			std::string table,
			std::vector<std::string> select_fields,
			std::vector<query_condition> conditions,
			std::vector<std::pair<std::string, sort_order>> order_by,
			std::optional<uint32_t> limit,
			std::optional<uint32_t> offset,
			std::vector<std::tuple<std::string, std::string, join_type>> joins,
			std::vector<std::string> group_by_fields,
			std::string having_clause
		);

		// Helper methods
		std::string escape_identifier(const std::string& identifier, database_types db_type) const;
		std::string format_value(const core::database_value& value, database_types db_type) const;
		std::string join_type_to_string(join_type type) const;
	};

} // namespace database
