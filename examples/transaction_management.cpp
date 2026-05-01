// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file transaction_management.cpp
 * @brief Demonstrates transaction management with commit and rollback.
 *
 * This example shows how to:
 * - Begin, commit, and rollback transactions
 * - Use the in_transaction() check
 * - Handle errors within a transaction scope
 * - Implement a simple RAII-style transaction guard pattern
 *
 * Prerequisites:
 * - A running PostgreSQL server
 * - Update the connection string below with valid credentials
 */

#include <iostream>
#include <memory>
#include <string>
#include <variant>

#include <kcenon/database/database_manager.h>
#include <kcenon/database/core/database_context.h>

using namespace database;

/**
 * @brief RAII transaction guard that rolls back on destruction unless committed.
 *
 * This pattern ensures that transactions are never left dangling even when
 * an exception or early return occurs.
 */
class transaction_guard
{
public:
	explicit transaction_guard(std::shared_ptr<database_manager> mgr)
		: mgr_(std::move(mgr)), committed_(false)
	{
		auto result = mgr_->begin_transaction();
		if (!result.is_ok())
		{
			throw std::runtime_error("Failed to begin transaction");
		}
	}

	~transaction_guard()
	{
		if (!committed_ && mgr_->in_transaction())
		{
			std::cout << "  [guard] Rolling back uncommitted transaction" << std::endl;
			mgr_->rollback_transaction();
		}
	}

	void commit()
	{
		auto result = mgr_->commit_transaction();
		if (result.is_ok())
		{
			committed_ = true;
		}
		else
		{
			throw std::runtime_error("Failed to commit transaction");
		}
	}

	// Non-copyable, non-movable
	transaction_guard(const transaction_guard&) = delete;
	transaction_guard& operator=(const transaction_guard&) = delete;

private:
	std::shared_ptr<database_manager> mgr_;
	bool committed_;
};

int main()
{
	std::cout << "=== transaction_management example ===" << std::endl;

	// Setup
	auto context = std::make_shared<database_context>();
	auto db_manager = std::make_shared<database_manager>(context);
	db_manager->set_mode(database_types::postgres);

	std::string connection_string
		= "host=localhost port=5432 dbname=example_db user=user password=password";

	auto connect_result = db_manager->connect_result(connection_string);
	if (!connect_result.is_ok())
	{
		std::cerr << "Connection failed. Ensure PostgreSQL is running." << std::endl;
		return 1;
	}
	std::cout << "Connected" << std::endl;

	// Prepare a test table
	db_manager->create_query_result(R"(
		CREATE TABLE IF NOT EXISTS accounts (
			id      SERIAL PRIMARY KEY,
			name    VARCHAR(100) NOT NULL,
			balance DECIMAL(12,2) NOT NULL DEFAULT 0.00
		)
	)");
	db_manager->execute_query_result("DELETE FROM accounts");
	db_manager->execute_query_result(
		"INSERT INTO accounts (name, balance) VALUES ('Alice', 1000.00)");
	db_manager->execute_query_result(
		"INSERT INTO accounts (name, balance) VALUES ('Bob',   500.00)");

	// -------------------------------------------------------
	// Scenario 1: Successful transaction (transfer funds)
	// -------------------------------------------------------
	std::cout << "\n--- Scenario 1: Successful transfer ---" << std::endl;
	{
		auto begin_result = db_manager->begin_transaction();
		if (!begin_result.is_ok())
		{
			std::cerr << "Failed to begin transaction" << std::endl;
			return 1;
		}
		std::cout << "Transaction started, in_transaction = "
				  << std::boolalpha << db_manager->in_transaction() << std::endl;

		// Transfer 200 from Alice to Bob
		db_manager->execute_query_result(
			"UPDATE accounts SET balance = balance - 200 WHERE name = 'Alice'");
		db_manager->execute_query_result(
			"UPDATE accounts SET balance = balance + 200 WHERE name = 'Bob'");

		auto commit_result = db_manager->commit_transaction();
		if (commit_result.is_ok())
		{
			std::cout << "Transaction committed" << std::endl;
		}

		// Verify balances
		auto result = db_manager->select_query_result(
			"SELECT name, balance FROM accounts ORDER BY name");
		if (result.is_ok())
		{
			for (const auto& row : result.value())
			{
				for (const auto& [col, val] : row)
				{
					std::cout << "  " << col << " = ";
					std::visit([](const auto& v) { std::cout << v; }, val);
					std::cout << "  ";
				}
				std::cout << std::endl;
			}
		}
	}

	// -------------------------------------------------------
	// Scenario 2: Rollback on error
	// -------------------------------------------------------
	std::cout << "\n--- Scenario 2: Rollback on error ---" << std::endl;
	{
		auto begin_result = db_manager->begin_transaction();
		if (!begin_result.is_ok())
		{
			std::cerr << "Failed to begin transaction" << std::endl;
			return 1;
		}

		// Debit Alice
		db_manager->execute_query_result(
			"UPDATE accounts SET balance = balance - 5000 WHERE name = 'Alice'");

		// Simulate a business rule check: Alice would go negative
		auto check = db_manager->select_query_result(
			"SELECT balance FROM accounts WHERE name = 'Alice'");

		bool should_rollback = false;
		if (check.is_ok() && !check.value().empty())
		{
			const auto& balance_val = check.value()[0].at("balance");
			// Check if balance went negative (as a string comparison for simplicity)
			std::visit(
				[&should_rollback](const auto& v)
				{
					using T = std::decay_t<decltype(v)>;
					if constexpr (std::is_same_v<T, double>)
					{
						should_rollback = (v < 0.0);
					}
					else if constexpr (std::is_same_v<T, std::string>)
					{
						// Some backends return numeric values as strings
						should_rollback = (!v.empty() && v[0] == '-');
					}
				},
				balance_val);
		}

		if (should_rollback)
		{
			std::cout << "Business rule violation detected, rolling back" << std::endl;
			db_manager->rollback_transaction();
			std::cout << "Transaction rolled back, in_transaction = "
					  << std::boolalpha << db_manager->in_transaction() << std::endl;
		}
		else
		{
			db_manager->commit_transaction();
			std::cout << "Transaction committed (balance was not negative)" << std::endl;
		}

		// Verify balances unchanged
		auto result = db_manager->select_query_result(
			"SELECT name, balance FROM accounts ORDER BY name");
		if (result.is_ok())
		{
			std::cout << "Balances after rollback:" << std::endl;
			for (const auto& row : result.value())
			{
				for (const auto& [col, val] : row)
				{
					std::cout << "  " << col << " = ";
					std::visit([](const auto& v) { std::cout << v; }, val);
					std::cout << "  ";
				}
				std::cout << std::endl;
			}
		}
	}

	// -------------------------------------------------------
	// Scenario 3: RAII transaction guard
	// -------------------------------------------------------
	std::cout << "\n--- Scenario 3: RAII transaction guard ---" << std::endl;
	{
		try
		{
			transaction_guard guard(db_manager);
			std::cout << "Guard started transaction" << std::endl;

			db_manager->execute_query_result(
				"INSERT INTO accounts (name, balance) VALUES ('Carol', 750.00)");

			// Commit explicitly through the guard
			guard.commit();
			std::cout << "Guard committed transaction" << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Transaction error: " << e.what() << std::endl;
		}

		// Verify Carol was added
		auto result = db_manager->select_query_result(
			"SELECT name, balance FROM accounts ORDER BY name");
		if (result.is_ok())
		{
			std::cout << "Accounts after guard commit:" << std::endl;
			for (const auto& row : result.value())
			{
				for (const auto& [col, val] : row)
				{
					std::cout << "  " << col << " = ";
					std::visit([](const auto& v) { std::cout << v; }, val);
					std::cout << "  ";
				}
				std::cout << std::endl;
			}
		}
	}

	// Cleanup
	db_manager->disconnect_result();
	std::cout << "\nDisconnected" << std::endl;

	std::cout << "=== transaction_management example completed ===" << std::endl;
	return 0;
}
