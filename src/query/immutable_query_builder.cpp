// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/query/immutable_query_builder.h>
#include <sstream>
#include <algorithm>

namespace kcenon::database
{
	// Public constructor
	immutable_query_builder::immutable_query_builder(const std::string& table)
		: table_(table)
		, select_fields_()
		, conditions_()
		, order_by_()
		, limit_(std::nullopt)
		, offset_(std::nullopt)
		, joins_()
		, group_by_fields_()
		, having_clause_()
	{
	}

	// Private constructor for copy-with-modification
	immutable_query_builder::immutable_query_builder(
		std::string table,
		std::vector<std::string> select_fields,
		std::vector<query_condition> conditions,
		std::vector<std::pair<std::string, sort_order>> order_by,
		std::optional<uint32_t> limit,
		std::optional<uint32_t> offset,
		std::vector<std::tuple<std::string, std::string, join_type>> joins,
		std::vector<std::string> group_by_fields,
		std::string having_clause
	)
		: table_(std::move(table))
		, select_fields_(std::move(select_fields))
		, conditions_(std::move(conditions))
		, order_by_(std::move(order_by))
		, limit_(limit)
		, offset_(offset)
		, joins_(std::move(joins))
		, group_by_fields_(std::move(group_by_fields))
		, having_clause_(std::move(having_clause))
	{
	}

	immutable_query_builder immutable_query_builder::select(std::vector<std::string> fields) const
	{
		return immutable_query_builder(
			table_,
			std::move(fields),
			conditions_,
			order_by_,
			limit_,
			offset_,
			joins_,
			group_by_fields_,
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::where(const std::string& field, const std::string& op, const core::database_value& value) const
	{
		auto new_conditions = conditions_;
		new_conditions.emplace_back(field, op, value);

		return immutable_query_builder(
			table_,
			select_fields_,
			std::move(new_conditions),
			order_by_,
			limit_,
			offset_,
			joins_,
			group_by_fields_,
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::where(const query_condition& condition) const
	{
		auto new_conditions = conditions_;
		new_conditions.push_back(condition);

		return immutable_query_builder(
			table_,
			select_fields_,
			std::move(new_conditions),
			order_by_,
			limit_,
			offset_,
			joins_,
			group_by_fields_,
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::order_by(const std::string& field, sort_order order) const
	{
		auto new_order_by = order_by_;
		new_order_by.emplace_back(field, order);

		return immutable_query_builder(
			table_,
			select_fields_,
			conditions_,
			std::move(new_order_by),
			limit_,
			offset_,
			joins_,
			group_by_fields_,
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::limit(uint32_t count) const
	{
		return immutable_query_builder(
			table_,
			select_fields_,
			conditions_,
			order_by_,
			count,
			offset_,
			joins_,
			group_by_fields_,
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::offset(uint32_t count) const
	{
		return immutable_query_builder(
			table_,
			select_fields_,
			conditions_,
			order_by_,
			limit_,
			count,
			joins_,
			group_by_fields_,
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::join(const std::string& table, const std::string& condition, join_type type) const
	{
		auto new_joins = joins_;
		new_joins.emplace_back(table, condition, type);

		return immutable_query_builder(
			table_,
			select_fields_,
			conditions_,
			order_by_,
			limit_,
			offset_,
			std::move(new_joins),
			group_by_fields_,
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::group_by(std::vector<std::string> fields) const
	{
		return immutable_query_builder(
			table_,
			select_fields_,
			conditions_,
			order_by_,
			limit_,
			offset_,
			joins_,
			std::move(fields),
			having_clause_
		);
	}

	immutable_query_builder immutable_query_builder::having(const std::string& condition) const
	{
		return immutable_query_builder(
			table_,
			select_fields_,
			conditions_,
			order_by_,
			limit_,
			offset_,
			joins_,
			group_by_fields_,
			condition
		);
	}

	std::string immutable_query_builder::build() const
	{
		return build_for_database(database_types::postgres);
	}

	std::string immutable_query_builder::build_for_database(database_types db_type) const
	{
		std::ostringstream query;

		// SELECT clause
		query << "SELECT ";
		if (select_fields_.empty())
		{
			query << "*";
		}
		else
		{
			for (size_t i = 0; i < select_fields_.size(); ++i)
			{
				if (i > 0)
					query << ", ";
				query << escape_identifier(select_fields_[i], db_type);
			}
		}

		// FROM clause
		query << " FROM " << escape_identifier(table_, db_type);

		// JOIN clauses
		for (const auto& [join_table, join_condition, join_type_val] : joins_)
		{
			query << " " << join_type_to_string(join_type_val) << " JOIN "
				  << escape_identifier(join_table, db_type)
				  << " ON " << join_condition;
		}

		// WHERE clause
		if (!conditions_.empty())
		{
			query << " WHERE ";
			for (size_t i = 0; i < conditions_.size(); ++i)
			{
				if (i > 0)
					query << " AND ";
				query << conditions_[i].to_sql();
			}
		}

		// GROUP BY clause
		if (!group_by_fields_.empty())
		{
			query << " GROUP BY ";
			for (size_t i = 0; i < group_by_fields_.size(); ++i)
			{
				if (i > 0)
					query << ", ";
				query << escape_identifier(group_by_fields_[i], db_type);
			}
		}

		// HAVING clause
		if (!having_clause_.empty())
		{
			query << " HAVING " << having_clause_;
		}

		// ORDER BY clause
		if (!order_by_.empty())
		{
			query << " ORDER BY ";
			for (size_t i = 0; i < order_by_.size(); ++i)
			{
				if (i > 0)
					query << ", ";
				query << escape_identifier(order_by_[i].first, db_type) << " "
					  << (order_by_[i].second == sort_order::asc ? "ASC" : "DESC");
			}
		}

		// LIMIT clause
		if (limit_.has_value())
		{
			query << " LIMIT " << limit_.value();
		}

		// OFFSET clause
		if (offset_.has_value())
		{
			query << " OFFSET " << offset_.value();
		}

		return query.str();
	}

	std::string immutable_query_builder::escape_identifier(const std::string& identifier, database_types db_type) const
	{
		switch (db_type)
		{
		case database_types::postgres:
			return "\"" + identifier + "\"";
		case database_types::sqlite:
			return "\"" + identifier + "\"";
		default:
			return identifier;
		}
	}

	std::string immutable_query_builder::format_value(const core::database_value& value, database_types db_type) const
	{
		if (std::holds_alternative<std::string>(value))
		{
			const auto& str = std::get<std::string>(value);
			std::string escaped;
			escaped.reserve(str.size() + 2);
			escaped += "'";
			for (char c : str)
			{
				if (c == '\'')
					escaped += "''";
				else
					escaped += c;
			}
			escaped += "'";
			return escaped;
		}
		else if (std::holds_alternative<int64_t>(value))
		{
			return std::to_string(std::get<int64_t>(value));
		}
		else if (std::holds_alternative<double>(value))
		{
			return std::to_string(std::get<double>(value));
		}
		else if (std::holds_alternative<bool>(value))
		{
			return std::get<bool>(value) ? "TRUE" : "FALSE";
		}
		else if (std::holds_alternative<std::nullptr_t>(value))
		{
			return "NULL";
		}
		return "NULL";
	}

	std::string immutable_query_builder::join_type_to_string(join_type type) const
	{
		switch (type)
		{
		case join_type::inner:
			return "INNER";
		case join_type::left:
			return "LEFT";
		case join_type::right:
			return "RIGHT";
		case join_type::full_outer:
			return "FULL OUTER";
		case join_type::cross:
			return "CROSS";
		default:
			return "INNER";
		}
	}

} // namespace kcenon::database
