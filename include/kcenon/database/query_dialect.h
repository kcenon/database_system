// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/database_types.h>
#include <kcenon/database/core/database_backend.h>

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace kcenon::database
{
	// Forward declarations
	class query_condition;
	enum class join_type;
	enum class sort_order;

	/**
	 * @class query_dialect
	 * @brief Abstract interface for database-specific query formatting.
	 *
	 * This class defines the Strategy interface for formatting queries
	 * according to different database dialects (SQL, MongoDB, Redis).
	 *
	 * ### Thread Safety
	 * - NOT thread-safe. Each thread should use its own instance.
	 * - Create separate dialect instances for each thread.
	 */
	class query_dialect
	{
	public:
		virtual ~query_dialect() = default;

		// Query type setting
		enum class query_type { none, select, insert, update, delete_query };
		virtual void set_query_type(query_type type) = 0;
		virtual query_type get_query_type() const = 0;

		// SELECT operations
		virtual void set_select_columns(const std::vector<std::string>& columns) = 0;
		virtual void set_from_table(const std::string& table) = 0;

		// WHERE conditions
		virtual void add_where_condition(const query_condition& condition) = 0;
		virtual void add_where_condition(const std::string& field, const std::string& op, const core::database_value& value) = 0;

		// JOIN operations
		virtual void add_join(const std::string& table, const std::string& condition, join_type type) = 0;

		// GROUP BY and HAVING
		virtual void set_group_by(const std::vector<std::string>& columns) = 0;
		virtual void set_having(const std::string& condition) = 0;

		// ORDER BY
		virtual void add_order_by(const std::string& column, sort_order order) = 0;

		// LIMIT and OFFSET
		virtual void set_limit(size_t count) = 0;
		virtual void set_offset(size_t count) = 0;

		// INSERT operations
		virtual void set_insert_table(const std::string& table) = 0;
		virtual void set_insert_data(const std::map<std::string, core::database_value>& data) = 0;
		virtual void set_insert_rows(const std::vector<std::map<std::string, core::database_value>>& rows) = 0;

		// UPDATE operations
		virtual void set_update_table(const std::string& table) = 0;
		virtual void set_update_data(const std::map<std::string, core::database_value>& data) = 0;

		// DELETE operations
		virtual void set_delete_table(const std::string& table) = 0;

		// Collection/Key operations (for NoSQL)
		virtual void set_collection(const std::string& name) = 0;
		virtual void set_key(const std::string& key) = 0;

		// Build final query
		virtual std::string build() const = 0;

		// Reset state
		virtual void reset() = 0;

		// Get database type
		virtual database_types get_database_type() const = 0;
	};

	namespace detail
	{
		/**
		 * @class sql_dialect
		 * @brief SQL dialect implementation for PostgreSQL, SQLite.
		 */
		class sql_dialect final : public query_dialect
		{
		public:
			explicit sql_dialect(database_types db_type);
			~sql_dialect() override = default;

			void set_query_type(query_type type) override;
			query_type get_query_type() const override;

			void set_select_columns(const std::vector<std::string>& columns) override;
			void set_from_table(const std::string& table) override;

			void add_where_condition(const query_condition& condition) override;
			void add_where_condition(const std::string& field, const std::string& op, const core::database_value& value) override;

			void add_join(const std::string& table, const std::string& condition, join_type type) override;

			void set_group_by(const std::vector<std::string>& columns) override;
			void set_having(const std::string& condition) override;

			void add_order_by(const std::string& column, sort_order order) override;

			void set_limit(size_t count) override;
			void set_offset(size_t count) override;

			void set_insert_table(const std::string& table) override;
			void set_insert_data(const std::map<std::string, core::database_value>& data) override;
			void set_insert_rows(const std::vector<std::map<std::string, core::database_value>>& rows) override;

			void set_update_table(const std::string& table) override;
			void set_update_data(const std::map<std::string, core::database_value>& data) override;

			void set_delete_table(const std::string& table) override;

			void set_collection(const std::string& name) override;
			void set_key(const std::string& key) override;

			std::string build() const override;

			void reset() override;

			database_types get_database_type() const override;

		private:
			database_types db_type_;
			query_type type_;
			std::vector<std::string> select_columns_;
			std::string from_table_;
			std::vector<query_condition> where_conditions_;
			std::vector<std::string> joins_;
			std::vector<std::string> group_by_columns_;
			std::string having_clause_;
			std::vector<std::string> order_by_clauses_;
			size_t limit_count_;
			size_t offset_count_;

			std::string target_table_;
			std::map<std::string, core::database_value> set_data_;
			std::vector<std::map<std::string, core::database_value>> insert_rows_;

			std::string escape_identifier(const std::string& identifier) const;
			std::string format_value(const core::database_value& value) const;
			std::string join_type_to_string(join_type type) const;
		};

#ifdef USE_MONGODB
		/**
		 * @class mongodb_dialect
		 * @brief MongoDB dialect implementation (experimental).
		 *
		 * @note This dialect is experimental and disabled by default.
		 *       Enable with CMake option USE_MONGODB=ON.
		 */
		class mongodb_dialect final : public query_dialect
		{
		public:
			mongodb_dialect();
			~mongodb_dialect() override = default;

			void set_query_type(query_type type) override;
			query_type get_query_type() const override;

			void set_select_columns(const std::vector<std::string>& columns) override;
			void set_from_table(const std::string& table) override;

			void add_where_condition(const query_condition& condition) override;
			void add_where_condition(const std::string& field, const std::string& op, const core::database_value& value) override;

			void add_join(const std::string& table, const std::string& condition, join_type type) override;

			void set_group_by(const std::vector<std::string>& columns) override;
			void set_having(const std::string& condition) override;

			void add_order_by(const std::string& column, sort_order order) override;

			void set_limit(size_t count) override;
			void set_offset(size_t count) override;

			void set_insert_table(const std::string& table) override;
			void set_insert_data(const std::map<std::string, core::database_value>& data) override;
			void set_insert_rows(const std::vector<std::map<std::string, core::database_value>>& rows) override;

			void set_update_table(const std::string& table) override;
			void set_update_data(const std::map<std::string, core::database_value>& data) override;

			void set_delete_table(const std::string& table) override;

			void set_collection(const std::string& name) override;
			void set_key(const std::string& key) override;

			std::string build() const override;

			void reset() override;

			database_types get_database_type() const override;

		private:
			enum class operation_type { none, find, insert, update, delete_op, aggregate };

			operation_type op_type_;
			std::string collection_name_;
			std::map<std::string, core::database_value> filter_;
			std::map<std::string, core::database_value> projection_;
			std::map<std::string, int> sort_spec_;
			size_t limit_count_;
			size_t skip_count_;

			std::map<std::string, core::database_value> document_;
			std::vector<std::map<std::string, core::database_value>> documents_;
			std::map<std::string, core::database_value> update_spec_;

			std::vector<std::map<std::string, core::database_value>> pipeline_;

			std::string to_json(const std::map<std::string, core::database_value>& data) const;
			std::string value_to_json(const core::database_value& value) const;
		};
#endif // USE_MONGODB

#ifdef USE_REDIS
		/**
		 * @class redis_dialect
		 * @brief Redis dialect implementation (experimental).
		 *
		 * @note This dialect is experimental and disabled by default.
		 *       Enable with CMake option USE_REDIS=ON.
		 */
		class redis_dialect final : public query_dialect
		{
		public:
			redis_dialect();
			~redis_dialect() override = default;

			void set_query_type(query_type type) override;
			query_type get_query_type() const override;

			void set_select_columns(const std::vector<std::string>& columns) override;
			void set_from_table(const std::string& table) override;

			void add_where_condition(const query_condition& condition) override;
			void add_where_condition(const std::string& field, const std::string& op, const core::database_value& value) override;

			void add_join(const std::string& table, const std::string& condition, join_type type) override;

			void set_group_by(const std::vector<std::string>& columns) override;
			void set_having(const std::string& condition) override;

			void add_order_by(const std::string& column, sort_order order) override;

			void set_limit(size_t count) override;
			void set_offset(size_t count) override;

			void set_insert_table(const std::string& table) override;
			void set_insert_data(const std::map<std::string, core::database_value>& data) override;
			void set_insert_rows(const std::vector<std::map<std::string, core::database_value>>& rows) override;

			void set_update_table(const std::string& table) override;
			void set_update_data(const std::map<std::string, core::database_value>& data) override;

			void set_delete_table(const std::string& table) override;

			void set_collection(const std::string& name) override;
			void set_key(const std::string& key) override;

			std::string build() const override;

			void reset() override;

			database_types get_database_type() const override;

			std::vector<std::string> build_args() const;

		private:
			std::string command_;
			std::vector<std::string> args_;
			std::string key_;
			std::string value_;
		};
#endif // USE_REDIS

	} // namespace detail

	/**
	 * @brief Factory function to create appropriate dialect for database type.
	 * @param type The database type.
	 * @return Unique pointer to the appropriate dialect implementation.
	 * @throws std::invalid_argument if database type is not supported.
	 */
	std::unique_ptr<query_dialect> create_dialect(database_types type);

} // namespace kcenon::database
