/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "query_dialect.h"
#include "query_builder.h"

#include <sstream>
#include <stdexcept>

namespace database
{
	std::unique_ptr<query_dialect> create_dialect(database_types type)
	{
		switch (type) {
			case database_types::postgres:
			case database_types::sqlite:
				return std::make_unique<detail::sql_dialect>(type);
#ifdef USE_MONGODB
			case database_types::mongodb:
				return std::make_unique<detail::mongodb_dialect>();
#endif
#ifdef USE_REDIS
			case database_types::redis:
				return std::make_unique<detail::redis_dialect>();
#endif
			default:
				throw std::invalid_argument("Unsupported database type");
		}
	}

	namespace detail
	{
		// sql_dialect implementation
		sql_dialect::sql_dialect(database_types db_type)
			: db_type_(db_type)
			, type_(query_type::none)
			, limit_count_(0)
			, offset_count_(0)
		{
		}

		void sql_dialect::set_query_type(query_type type)
		{
			type_ = type;
		}

		query_dialect::query_type sql_dialect::get_query_type() const
		{
			return type_;
		}

		void sql_dialect::set_select_columns(const std::vector<std::string>& columns)
		{
			type_ = query_type::select;
			select_columns_ = columns;
		}

		void sql_dialect::set_from_table(const std::string& table)
		{
			from_table_ = table;
		}

		void sql_dialect::add_where_condition(const query_condition& condition)
		{
			where_conditions_.push_back(condition);
		}

		void sql_dialect::add_where_condition(const std::string& field, const std::string& op, const core::database_value& value)
		{
			where_conditions_.emplace_back(field, op, value);
		}

		void sql_dialect::add_join(const std::string& table, const std::string& condition, join_type type)
		{
			std::ostringstream oss;
			oss << join_type_to_string(type) << " JOIN " << table << " ON " << condition;
			joins_.push_back(oss.str());
		}

		void sql_dialect::set_group_by(const std::vector<std::string>& columns)
		{
			group_by_columns_ = columns;
		}

		void sql_dialect::set_having(const std::string& condition)
		{
			having_clause_ = condition;
		}

		void sql_dialect::add_order_by(const std::string& column, sort_order order)
		{
			std::ostringstream oss;
			oss << column << " " << (order == sort_order::asc ? "ASC" : "DESC");
			order_by_clauses_.push_back(oss.str());
		}

		void sql_dialect::set_limit(size_t count)
		{
			limit_count_ = count;
		}

		void sql_dialect::set_offset(size_t count)
		{
			offset_count_ = count;
		}

		void sql_dialect::set_insert_table(const std::string& table)
		{
			type_ = query_type::insert;
			target_table_ = table;
		}

		void sql_dialect::set_insert_data(const std::map<std::string, core::database_value>& data)
		{
			set_data_ = data;
		}

		void sql_dialect::set_insert_rows(const std::vector<std::map<std::string, core::database_value>>& rows)
		{
			insert_rows_ = rows;
		}

		void sql_dialect::set_update_table(const std::string& table)
		{
			type_ = query_type::update;
			target_table_ = table;
		}

		void sql_dialect::set_update_data(const std::map<std::string, core::database_value>& data)
		{
			set_data_ = data;
		}

		void sql_dialect::set_delete_table(const std::string& table)
		{
			type_ = query_type::delete_query;
			target_table_ = table;
		}

		void sql_dialect::set_collection(const std::string& /*name*/)
		{
			// No-op for SQL
		}

		void sql_dialect::set_key(const std::string& /*key*/)
		{
			// No-op for SQL
		}

		std::string sql_dialect::build() const
		{
			std::ostringstream oss;

			switch (type_) {
				case query_type::select:
					oss << "SELECT ";
					if (select_columns_.empty()) {
						oss << "*";
					} else {
						for (size_t i = 0; i < select_columns_.size(); ++i) {
							if (i > 0) oss << ", ";
							oss << escape_identifier(select_columns_[i]);
						}
					}
					if (!from_table_.empty()) {
						oss << " FROM " << escape_identifier(from_table_);
					}
					break;

				case query_type::insert:
					oss << "INSERT INTO " << escape_identifier(target_table_);
					if (!insert_rows_.empty()) {
						auto& first_row = insert_rows_[0];
						oss << " (";
						bool first = true;
						for (const auto& pair : first_row) {
							if (!first) oss << ", ";
							oss << escape_identifier(pair.first);
							first = false;
						}
						oss << ") VALUES ";
						for (size_t i = 0; i < insert_rows_.size(); ++i) {
							if (i > 0) oss << ", ";
							oss << "(";
							bool first_val = true;
							for (const auto& pair : insert_rows_[i]) {
								if (!first_val) oss << ", ";
								oss << format_value(pair.second);
								first_val = false;
							}
							oss << ")";
						}
					} else if (!set_data_.empty()) {
						oss << " (";
						bool first = true;
						for (const auto& pair : set_data_) {
							if (!first) oss << ", ";
							oss << escape_identifier(pair.first);
							first = false;
						}
						oss << ") VALUES (";
						first = true;
						for (const auto& pair : set_data_) {
							if (!first) oss << ", ";
							oss << format_value(pair.second);
							first = false;
						}
						oss << ")";
					}
					break;

				case query_type::update:
					oss << "UPDATE " << escape_identifier(target_table_) << " SET ";
					{
						bool first = true;
						for (const auto& pair : set_data_) {
							if (!first) oss << ", ";
							oss << escape_identifier(pair.first) << " = " << format_value(pair.second);
							first = false;
						}
					}
					break;

				case query_type::delete_query:
					oss << "DELETE FROM " << escape_identifier(target_table_);
					break;

				default:
					throw std::runtime_error("Invalid query type");
			}

			// Add JOINs
			for (const auto& join : joins_) {
				oss << " " << join;
			}

			// Add WHERE clause
			if (!where_conditions_.empty()) {
				oss << " WHERE ";
				for (size_t i = 0; i < where_conditions_.size(); ++i) {
					if (i > 0) oss << " AND ";
					oss << where_conditions_[i].to_sql();
				}
			}

			// Add GROUP BY
			if (!group_by_columns_.empty()) {
				oss << " GROUP BY ";
				for (size_t i = 0; i < group_by_columns_.size(); ++i) {
					if (i > 0) oss << ", ";
					oss << escape_identifier(group_by_columns_[i]);
				}
			}

			// Add HAVING
			if (!having_clause_.empty()) {
				oss << " HAVING " << having_clause_;
			}

			// Add ORDER BY
			if (!order_by_clauses_.empty()) {
				oss << " ORDER BY ";
				for (size_t i = 0; i < order_by_clauses_.size(); ++i) {
					if (i > 0) oss << ", ";
					oss << order_by_clauses_[i];
				}
			}

			// Add LIMIT and OFFSET
			if (limit_count_ > 0) {
				oss << " LIMIT " << limit_count_;
			}
			if (offset_count_ > 0) {
				oss << " OFFSET " << offset_count_;
			}

			return oss.str();
		}

		void sql_dialect::reset()
		{
			type_ = query_type::none;
			select_columns_.clear();
			from_table_.clear();
			where_conditions_.clear();
			joins_.clear();
			group_by_columns_.clear();
			having_clause_.clear();
			order_by_clauses_.clear();
			limit_count_ = 0;
			offset_count_ = 0;
			target_table_.clear();
			set_data_.clear();
			insert_rows_.clear();
		}

		database_types sql_dialect::get_database_type() const
		{
			return db_type_;
		}

		std::string sql_dialect::escape_identifier(const std::string& identifier) const
		{
			switch (db_type_) {
				case database_types::postgres:
					return "\"" + identifier + "\"";
				case database_types::sqlite:
					return "[" + identifier + "]";
				default:
					return identifier;
			}
		}

		std::string sql_dialect::format_value(const core::database_value& value) const
		{
			std::ostringstream oss;
			std::visit([&oss](const auto& val) {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, std::string>) {
					// Escape single quotes by doubling them (SQL standard)
					std::string escaped;
					escaped.reserve(val.size() + 10);
					for (char c : val) {
						if (c == '\'') {
							escaped += "''";
						} else {
							escaped += c;
						}
					}
					oss << "'" << escaped << "'";
				} else if constexpr (std::is_same_v<T, int64_t>) {
					oss << val;
				} else if constexpr (std::is_same_v<T, double>) {
					oss << val;
				} else if constexpr (std::is_same_v<T, bool>) {
					oss << (val ? "TRUE" : "FALSE");
				} else if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, std::nullptr_t>) {
					oss << "NULL";
				}
			}, value);
			return oss.str();
		}

		std::string sql_dialect::join_type_to_string(join_type type) const
		{
			switch (type) {
				case join_type::inner: return "INNER";
				case join_type::left: return "LEFT";
				case join_type::right: return "RIGHT";
				case join_type::full_outer: return "FULL OUTER";
				case join_type::cross: return "CROSS";
				default: return "INNER";
			}
		}

#ifdef USE_MONGODB
		// mongodb_dialect implementation
		mongodb_dialect::mongodb_dialect()
			: op_type_(operation_type::none)
			, limit_count_(0)
			, skip_count_(0)
		{
		}

		void mongodb_dialect::set_query_type(query_type type)
		{
			switch (type) {
				case query_type::select:
					op_type_ = operation_type::find;
					break;
				case query_type::insert:
					op_type_ = operation_type::insert;
					break;
				case query_type::update:
					op_type_ = operation_type::update;
					break;
				case query_type::delete_query:
					op_type_ = operation_type::delete_op;
					break;
				default:
					op_type_ = operation_type::none;
					break;
			}
		}

		query_dialect::query_type mongodb_dialect::get_query_type() const
		{
			switch (op_type_) {
				case operation_type::find:
					return query_type::select;
				case operation_type::insert:
					return query_type::insert;
				case operation_type::update:
					return query_type::update;
				case operation_type::delete_op:
					return query_type::delete_query;
				default:
					return query_type::none;
			}
		}

		void mongodb_dialect::set_select_columns(const std::vector<std::string>& columns)
		{
			op_type_ = operation_type::find;
			projection_.clear();
			for (const auto& field : columns) {
				projection_[field] = core::database_value{int64_t(1)};
			}
		}

		void mongodb_dialect::set_from_table(const std::string& table)
		{
			collection_name_ = table;
		}

		void mongodb_dialect::add_where_condition(const query_condition& /*condition*/)
		{
			// MongoDB uses filter_ directly
		}

		void mongodb_dialect::add_where_condition(const std::string& field, const std::string& op, const core::database_value& value)
		{
			if (op == "=") {
				filter_[field] = value;
			} else {
				// For other operators, we need to build a nested structure
				// This is a simplified implementation
				filter_[field] = value;
			}
		}

		void mongodb_dialect::add_join(const std::string& /*table*/, const std::string& /*condition*/, join_type /*type*/)
		{
			// MongoDB doesn't support JOINs in the same way as SQL
			// Use aggregation pipeline with $lookup instead
		}

		void mongodb_dialect::set_group_by(const std::vector<std::string>& /*columns*/)
		{
			// Use aggregation pipeline with $group
			op_type_ = operation_type::aggregate;
		}

		void mongodb_dialect::set_having(const std::string& /*condition*/)
		{
			// Part of aggregation pipeline
		}

		void mongodb_dialect::add_order_by(const std::string& column, sort_order order)
		{
			sort_spec_[column] = (order == sort_order::asc) ? 1 : -1;
		}

		void mongodb_dialect::set_limit(size_t count)
		{
			limit_count_ = count;
		}

		void mongodb_dialect::set_offset(size_t count)
		{
			skip_count_ = count;
		}

		void mongodb_dialect::set_insert_table(const std::string& table)
		{
			op_type_ = operation_type::insert;
			collection_name_ = table;
		}

		void mongodb_dialect::set_insert_data(const std::map<std::string, core::database_value>& data)
		{
			op_type_ = operation_type::insert;
			document_ = data;
		}

		void mongodb_dialect::set_insert_rows(const std::vector<std::map<std::string, core::database_value>>& rows)
		{
			op_type_ = operation_type::insert;
			documents_ = rows;
		}

		void mongodb_dialect::set_update_table(const std::string& table)
		{
			op_type_ = operation_type::update;
			collection_name_ = table;
		}

		void mongodb_dialect::set_update_data(const std::map<std::string, core::database_value>& data)
		{
			op_type_ = operation_type::update;
			update_spec_ = data;
		}

		void mongodb_dialect::set_delete_table(const std::string& table)
		{
			op_type_ = operation_type::delete_op;
			collection_name_ = table;
		}

		void mongodb_dialect::set_collection(const std::string& name)
		{
			collection_name_ = name;
		}

		void mongodb_dialect::set_key(const std::string& /*key*/)
		{
			// No-op for MongoDB
		}

		std::string mongodb_dialect::build() const
		{
			std::ostringstream oss;

			switch (op_type_) {
				case operation_type::find:
					oss << "db." << collection_name_ << ".find(";
					oss << to_json(filter_);
					if (!projection_.empty()) {
						oss << ", " << to_json(projection_);
					}
					oss << ")";
					if (!sort_spec_.empty()) {
						oss << ".sort({";
						bool first = true;
						for (const auto& pair : sort_spec_) {
							if (!first) oss << ", ";
							oss << "\"" << pair.first << "\": " << pair.second;
							first = false;
						}
						oss << "})";
					}
					if (skip_count_ > 0) {
						oss << ".skip(" << skip_count_ << ")";
					}
					if (limit_count_ > 0) {
						oss << ".limit(" << limit_count_ << ")";
					}
					break;

				case operation_type::insert:
					if (documents_.empty()) {
						oss << "db." << collection_name_ << ".insertOne(" << to_json(document_) << ")";
					} else {
						oss << "db." << collection_name_ << ".insertMany([";
						for (size_t i = 0; i < documents_.size(); ++i) {
							if (i > 0) oss << ", ";
							oss << to_json(documents_[i]);
						}
						oss << "])";
					}
					break;

				case operation_type::update:
					oss << "db." << collection_name_ << ".updateOne(";
					oss << to_json(filter_) << ", { \"$set\": " << to_json(update_spec_) << " })";
					break;

				case operation_type::delete_op:
					if (limit_count_ == 1) {
						oss << "db." << collection_name_ << ".deleteOne(" << to_json(filter_) << ")";
					} else {
						oss << "db." << collection_name_ << ".deleteMany(" << to_json(filter_) << ")";
					}
					break;

				case operation_type::aggregate:
					oss << "db." << collection_name_ << ".aggregate([";
					for (size_t i = 0; i < pipeline_.size(); ++i) {
						if (i > 0) oss << ", ";
						oss << to_json(pipeline_[i]);
					}
					oss << "])";
					break;

				default:
					throw std::runtime_error("Invalid MongoDB operation type");
			}

			return oss.str();
		}

		void mongodb_dialect::reset()
		{
			op_type_ = operation_type::none;
			collection_name_.clear();
			filter_.clear();
			projection_.clear();
			sort_spec_.clear();
			limit_count_ = 0;
			skip_count_ = 0;
			document_.clear();
			documents_.clear();
			update_spec_.clear();
			pipeline_.clear();
		}

		database_types mongodb_dialect::get_database_type() const
		{
			return database_types::mongodb;
		}

		std::string mongodb_dialect::to_json(const std::map<std::string, core::database_value>& data) const
		{
			if (data.empty()) {
				return "{}";
			}

			std::ostringstream oss;
			oss << "{ ";
			bool first = true;
			for (const auto& pair : data) {
				if (!first) oss << ", ";
				oss << "\"" << pair.first << "\": " << value_to_json(pair.second);
				first = false;
			}
			oss << " }";
			return oss.str();
		}

		std::string mongodb_dialect::value_to_json(const core::database_value& value) const
		{
			std::ostringstream oss;
			std::visit([&oss](const auto& val) {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, std::string>) {
					oss << "\"" << val << "\"";
				} else if constexpr (std::is_same_v<T, int64_t>) {
					oss << val;
				} else if constexpr (std::is_same_v<T, double>) {
					oss << val;
				} else if constexpr (std::is_same_v<T, bool>) {
					oss << (val ? "true" : "false");
				} else if constexpr (std::is_same_v<T, std::monostate> || std::is_same_v<T, std::nullptr_t>) {
					oss << "null";
				}
			}, value);
			return oss.str();
		}
#endif // USE_MONGODB

#ifdef USE_REDIS
		// redis_dialect implementation
		redis_dialect::redis_dialect()
		{
		}

		void redis_dialect::set_query_type(query_type /*type*/)
		{
			// Redis commands don't map directly to query types
		}

		query_dialect::query_type redis_dialect::get_query_type() const
		{
			return query_type::none;
		}

		void redis_dialect::set_select_columns(const std::vector<std::string>& /*columns*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_from_table(const std::string& /*table*/)
		{
			// No-op for Redis
		}

		void redis_dialect::add_where_condition(const query_condition& /*condition*/)
		{
			// No-op for Redis
		}

		void redis_dialect::add_where_condition(const std::string& /*field*/, const std::string& /*op*/, const core::database_value& /*value*/)
		{
			// No-op for Redis
		}

		void redis_dialect::add_join(const std::string& /*table*/, const std::string& /*condition*/, join_type /*type*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_group_by(const std::vector<std::string>& /*columns*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_having(const std::string& /*condition*/)
		{
			// No-op for Redis
		}

		void redis_dialect::add_order_by(const std::string& /*column*/, sort_order /*order*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_limit(size_t /*count*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_offset(size_t /*count*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_insert_table(const std::string& /*table*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_insert_data(const std::map<std::string, core::database_value>& data)
		{
			// For Redis, we can use HSET to insert hash data
			if (!data.empty()) {
				command_ = "HSET";
				args_.clear();
				args_.push_back(key_);
				for (const auto& pair : data) {
					args_.push_back(pair.first);
					std::visit([this](const auto& val) {
						using T = std::decay_t<decltype(val)>;
						if constexpr (std::is_same_v<T, std::string>) {
							args_.push_back(val);
						} else if constexpr (std::is_same_v<T, int64_t>) {
							args_.push_back(std::to_string(val));
						} else if constexpr (std::is_same_v<T, double>) {
							args_.push_back(std::to_string(val));
						} else if constexpr (std::is_same_v<T, bool>) {
							args_.push_back(val ? "1" : "0");
						} else {
							args_.push_back("");
						}
					}, pair.second);
				}
			}
		}

		void redis_dialect::set_insert_rows(const std::vector<std::map<std::string, core::database_value>>& /*rows*/)
		{
			// Redis doesn't support bulk insert in the same way
		}

		void redis_dialect::set_update_table(const std::string& /*table*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_update_data(const std::map<std::string, core::database_value>& data)
		{
			set_insert_data(data); // Same as insert for Redis HSET
		}

		void redis_dialect::set_delete_table(const std::string& /*table*/)
		{
			command_ = "DEL";
			args_.clear();
			args_.push_back(key_);
		}

		void redis_dialect::set_collection(const std::string& /*name*/)
		{
			// No-op for Redis
		}

		void redis_dialect::set_key(const std::string& key)
		{
			key_ = key;
			command_ = "GET";
			args_.clear();
			args_.push_back(key);
		}

		std::string redis_dialect::build() const
		{
			std::ostringstream oss;
			oss << command_;
			for (const auto& arg : args_) {
				oss << " " << arg;
			}
			return oss.str();
		}

		void redis_dialect::reset()
		{
			command_.clear();
			args_.clear();
			key_.clear();
			value_.clear();
		}

		database_types redis_dialect::get_database_type() const
		{
			return database_types::redis;
		}

		std::vector<std::string> redis_dialect::build_args() const
		{
			std::vector<std::string> result;
			result.push_back(command_);
			result.insert(result.end(), args_.begin(), args_.end());
			return result;
		}
#endif // USE_REDIS

	} // namespace detail

} // namespace database
