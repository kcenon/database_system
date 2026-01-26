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

#include "query_builder.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace database
{
	// query_condition implementation
	query_condition::query_condition(const std::string& field, const std::string& op, const core::database_value& value)
		: field_(field), operator_(op), value_(value)
	{
	}

	query_condition::query_condition(const std::string& raw_condition)
		: raw_condition_(raw_condition)
	{
	}

	std::string query_condition::to_sql() const
	{
		if (!raw_condition_.empty()) {
			return raw_condition_;
		}

		if (!sub_conditions_.empty()) {
			std::ostringstream oss;
			oss << "(";
			for (size_t i = 0; i < sub_conditions_.size(); ++i) {
				if (i > 0) {
					oss << " " << logical_operator_ << " ";
				}
				oss << sub_conditions_[i].to_sql();
			}
			oss << ")";
			return oss.str();
		}

		std::ostringstream oss;
		oss << field_ << " " << operator_ << " ";

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
			} else if constexpr (std::is_same_v<T, std::monostate>) {
				oss << "NULL";
			}
		}, value_);

		return oss.str();
	}

	std::string query_condition::to_mongodb() const
	{
		if (!raw_condition_.empty()) {
			return raw_condition_;
		}

		if (!sub_conditions_.empty()) {
			std::ostringstream oss;
			std::string mongo_op = (logical_operator_ == "AND") ? "$and" : "$or";
			oss << "{ \"" << mongo_op << "\": [";
			for (size_t i = 0; i < sub_conditions_.size(); ++i) {
				if (i > 0) oss << ", ";
				oss << sub_conditions_[i].to_mongodb();
			}
			oss << "] }";
			return oss.str();
		}

		std::ostringstream oss;
		oss << "{ \"" << field_ << "\": ";

		if (operator_ == "=") {
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
				} else if constexpr (std::is_same_v<T, std::monostate>) {
					oss << "null";
				}
			}, value_);
		} else {
			std::string mongo_op;
			if (operator_ == ">") mongo_op = "$gt";
			else if (operator_ == ">=") mongo_op = "$gte";
			else if (operator_ == "<") mongo_op = "$lt";
			else if (operator_ == "<=") mongo_op = "$lte";
			else if (operator_ == "!=") mongo_op = "$ne";
			else mongo_op = "$eq";

			oss << "{ \"" << mongo_op << "\": ";
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
				} else if constexpr (std::is_same_v<T, std::monostate>) {
					oss << "null";
				}
			}, value_);
			oss << " }";
		}

		oss << " }";
		return oss.str();
	}

	std::string query_condition::to_redis() const
	{
		return raw_condition_;
	}

	query_condition query_condition::operator&&(const query_condition& other) const
	{
		query_condition result("");
		result.sub_conditions_.push_back(*this);
		result.sub_conditions_.push_back(other);
		result.logical_operator_ = "AND";
		return result;
	}

	query_condition query_condition::operator||(const query_condition& other) const
	{
		query_condition result("");
		result.sub_conditions_.push_back(*this);
		result.sub_conditions_.push_back(other);
		result.logical_operator_ = "OR";
		return result;
	}

	// query_builder implementation using Strategy pattern
	query_builder::query_builder(database_types db_type)
		: db_type_(db_type)
	{
		ensure_dialect();
	}

	query_builder& query_builder::for_database(database_types db_type)
	{
		if (db_type_ != db_type) {
			db_type_ = db_type;
			dialect_.reset();
			ensure_dialect();
		}
		return *this;
	}

	query_builder& query_builder::select(const std::vector<std::string>& columns)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_select_columns(columns);
		}
		return *this;
	}

	query_builder& query_builder::from(const std::string& table)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_from_table(table);
		}
		return *this;
	}

	query_builder& query_builder::where(const std::string& field, const std::string& op, const core::database_value& value)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->add_where_condition(field, op, value);
		}
		return *this;
	}

	query_builder& query_builder::where(const query_condition& condition)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->add_where_condition(condition);
		}
		return *this;
	}

	query_builder& query_builder::join(const std::string& table, const std::string& condition, join_type type)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->add_join(table, condition, type);
		}
		return *this;
	}

	query_builder& query_builder::order_by(const std::string& column, sort_order order)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->add_order_by(column, order);
		}
		return *this;
	}

	query_builder& query_builder::group_by(const std::vector<std::string>& columns)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_group_by(columns);
		}
		return *this;
	}

	query_builder& query_builder::group_by(const std::string& column)
	{
		return group_by(std::vector<std::string>{column});
	}

	query_builder& query_builder::having(const std::string& condition)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_having(condition);
		}
		return *this;
	}

	query_builder& query_builder::limit(size_t count)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_limit(count);
		}
		return *this;
	}

	query_builder& query_builder::offset(size_t count)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_offset(count);
		}
		return *this;
	}

	query_builder& query_builder::insert_into(const std::string& table)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_insert_table(table);
		}
		return *this;
	}

	query_builder& query_builder::values(const std::map<std::string, core::database_value>& data)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_insert_data(data);
		}
		return *this;
	}

	query_builder& query_builder::values(const std::vector<std::map<std::string, core::database_value>>& rows)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_insert_rows(rows);
		}
		return *this;
	}

	query_builder& query_builder::update(const std::string& table)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_update_table(table);
		}
		return *this;
	}

	query_builder& query_builder::set(const std::string& field, const core::database_value& value)
	{
		ensure_dialect();
		if (dialect_) {
			std::map<std::string, core::database_value> data;
			data[field] = value;
			dialect_->set_update_data(data);
		}
		return *this;
	}

	query_builder& query_builder::set(const std::map<std::string, core::database_value>& data)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_update_data(data);
		}
		return *this;
	}

	query_builder& query_builder::delete_from(const std::string& table)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_delete_table(table);
		}
		return *this;
	}

	query_builder& query_builder::collection(const std::string& name)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_collection(name);
		}
		return *this;
	}

	query_builder& query_builder::key(const std::string& key)
	{
		ensure_dialect();
		if (dialect_) {
			dialect_->set_key(key);
		}
		return *this;
	}

	std::string query_builder::build() const
	{
		if (dialect_) {
			return dialect_->build();
		}
		return "";
	}

	core::database_result query_builder::execute(core::database_backend* db) const
	{
		if (!db) {
			return {};
		}

		std::string query = build();
		if (query.empty()) {
			return {};
		}

		auto result = db->select_query(query);
		if (result.is_err()) {
			return {};
		}
		return result.value();
	}

	void query_builder::reset()
	{
		if (dialect_) {
			dialect_->reset();
		}
	}

	database_types query_builder::get_database_type() const
	{
		return db_type_;
	}

	void query_builder::ensure_dialect()
	{
		if (!dialect_ && db_type_ != database_types::none) {
			dialect_ = create_dialect(db_type_);
		}
	}

} // namespace database
