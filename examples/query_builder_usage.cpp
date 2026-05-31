// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_builder_usage.cpp
 * @brief Demonstrates the fluent query builder API for constructing SQL queries.
 *
 * This example shows how to:
 * - Build SELECT queries with WHERE, ORDER BY, LIMIT, and OFFSET
 * - Build INSERT queries with column-value maps
 * - Build UPDATE queries with SET and WHERE clauses
 * - Build DELETE queries with conditions
 * - Use JOIN, GROUP BY, and HAVING clauses
 * - Use query_condition objects with logical operators
 *
 * Note: This example generates SQL strings without executing them. It does
 * not require a running database and is useful for understanding the query
 * builder API.
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>

#include <kcenon/database/query_builder.h>
#include <kcenon/database/database_types.h>

using namespace kcenon::database;

int main()
{
	std::cout << "=== query_builder_usage example ===" << std::endl;

	// -------------------------------------------------------
	// 1. SELECT query
	// -------------------------------------------------------
	std::cout << "\n--- SELECT ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		auto query = builder.select({"id", "name", "email"})
						 .from("users")
						 .where("age", ">=", int64_t{18})
						 .where("is_active", "=", true)
						 .order_by("name", sort_order::asc)
						 .limit(20)
						 .offset(0)
						 .build();

		std::cout << "SELECT query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 2. INSERT query
	// -------------------------------------------------------
	std::cout << "\n--- INSERT ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		std::map<std::string, core::database_value> row;
		row["name"] = std::string("Alice");
		row["email"] = std::string("alice@example.com");
		row["age"] = int64_t{30};
		row["is_active"] = true;

		auto query = builder.insert_into("users").values(row).build();

		std::cout << "INSERT query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 3. Batch INSERT query
	// -------------------------------------------------------
	std::cout << "\n--- Batch INSERT ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		std::vector<std::map<std::string, core::database_value>> rows;

		std::map<std::string, core::database_value> row1;
		row1["name"] = std::string("Bob");
		row1["email"] = std::string("bob@example.com");
		row1["age"] = int64_t{25};
		rows.push_back(row1);

		std::map<std::string, core::database_value> row2;
		row2["name"] = std::string("Carol");
		row2["email"] = std::string("carol@example.com");
		row2["age"] = int64_t{28};
		rows.push_back(row2);

		auto query = builder.insert_into("users").values(rows).build();

		std::cout << "Batch INSERT query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 4. UPDATE query
	// -------------------------------------------------------
	std::cout << "\n--- UPDATE ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		auto query = builder.update("users")
						 .set("email", std::string("newalice@example.com"))
						 .set("age", int64_t{31})
						 .where("name", "=", std::string("Alice"))
						 .build();

		std::cout << "UPDATE query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 5. DELETE query
	// -------------------------------------------------------
	std::cout << "\n--- DELETE ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		auto query = builder.delete_from("users")
						 .where("is_active", "=", false)
						 .build();

		std::cout << "DELETE query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 6. JOIN query
	// -------------------------------------------------------
	std::cout << "\n--- JOIN ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		auto query = builder.select({"u.name", "o.total"})
						 .from("users u")
						 .join("orders o", "u.id = o.user_id", join_type::inner)
						 .where("o.total", ">", 100.0)
						 .order_by("o.total", sort_order::desc)
						 .build();

		std::cout << "JOIN query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 7. GROUP BY with HAVING
	// -------------------------------------------------------
	std::cout << "\n--- GROUP BY ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		auto query = builder.select({"department", "COUNT(*) AS cnt"})
						 .from("employees")
						 .group_by("department")
						 .having("COUNT(*) > 5")
						 .order_by("cnt", sort_order::desc)
						 .build();

		std::cout << "GROUP BY query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 8. Compound conditions with query_condition
	// -------------------------------------------------------
	std::cout << "\n--- Compound conditions ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		// Build individual conditions
		query_condition age_check("age", ">=", int64_t{21});
		query_condition active_check("is_active", "=", true);

		// Combine with logical AND
		auto combined = age_check && active_check;

		auto query = builder.select({"*"}).from("users").where(combined).build();

		std::cout << "Compound condition query:\n  " << query << std::endl;
	}

	// -------------------------------------------------------
	// 9. Switching database dialect
	// -------------------------------------------------------
	std::cout << "\n--- Dialect switch ---" << std::endl;
	{
		query_builder builder(database_types::postgres);

		auto pg_query = builder.select({"id", "name"})
							.from("users")
							.where("id", "=", int64_t{1})
							.build();
		std::cout << "PostgreSQL: " << pg_query << std::endl;

		// Reset and switch dialect to SQLite
		builder.reset();
		builder.for_database(database_types::sqlite);

		auto sqlite_query = builder.select({"id", "name"})
							   .from("users")
							   .where("id", "=", int64_t{1})
							   .build();
		std::cout << "SQLite:     " << sqlite_query << std::endl;
	}

	std::cout << "\n=== query_builder_usage example completed ===" << std::endl;
	return 0;
}
