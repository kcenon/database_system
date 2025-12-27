// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file database_base_adapter.h
 * @brief Adapter for migrating from database_base to database_backend
 *
 * This adapter allows legacy code using database_base to work with the new
 * database_backend implementation. It provides a gradual migration path for
 * existing code.
 *
 * Usage:
 * @code
 *   // Create a database_backend instance
 *   auto backend = backend_registry::instance().create("postgresql");
 *
 *   // Wrap it for legacy code compatibility
 *   auto adapter = std::make_shared<database_base_adapter>(std::move(backend));
 *
 *   // Use adapter with legacy code expecting database_base
 *   legacy_function(adapter);
 * @endcode
 *
 * @see database_base (deprecated)
 * @see database_backend (recommended)
 * @see docs/MIGRATION_database_base.md
 */

#pragma once

// Suppress deprecation warnings for this file since we're implementing
// the deprecated interface
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#include "database_base.h"
#include "core/database_backend.h"

#include <memory>
#include <string>

namespace database
{

/**
 * @class database_base_adapter
 * @brief Adapts database_backend to database_base interface
 *
 * This adapter wraps a database_backend instance and exposes the legacy
 * database_base interface. Use this class to integrate new database_backend
 * implementations with existing code that expects database_base.
 *
 * Design Pattern: Adapter pattern
 * - Wraps database_backend (new implementation)
 * - Exposes database_base interface (legacy)
 * - Converts Result<T> to bool/unsigned int return types
 *
 * Error Handling:
 * - Success from database_backend maps to true or row count
 * - Errors from database_backend map to false or 0
 * - Use last_error() on the underlying backend for error details
 *
 * Thread Safety:
 * - Same thread safety guarantees as the underlying database_backend
 *
 * @note This adapter is intended for migration purposes only.
 *       New code should use database_backend directly.
 */
class database_base_adapter : public database_base
{
public:
	/**
	 * @brief Construct adapter with a database_backend
	 * @param backend The database_backend to wrap (takes ownership)
	 * @throws std::invalid_argument if backend is nullptr
	 */
	explicit database_base_adapter(std::unique_ptr<core::database_backend> backend)
		: backend_(std::move(backend))
	{
		if (!backend_)
		{
			throw std::invalid_argument("database_base_adapter: backend cannot be nullptr");
		}
	}

	/**
	 * @brief Destructor - ensures proper cleanup of backend
	 */
	~database_base_adapter() override = default;

	// Prevent copying (unique ownership of backend)
	database_base_adapter(const database_base_adapter&) = delete;
	database_base_adapter& operator=(const database_base_adapter&) = delete;

	// Allow moving
	database_base_adapter(database_base_adapter&&) noexcept = default;
	database_base_adapter& operator=(database_base_adapter&&) noexcept = default;

	/**
	 * @brief Get the database type
	 * @return Database type from the underlying backend
	 */
	database_types database_type() override
	{
		return backend_->type();
	}

	/**
	 * @brief Connect to database using connection string
	 * @param connect_string Connection string (format depends on backend)
	 * @return true on success, false on failure
	 *
	 * The connection string is parsed using connection_config::from_string().
	 */
	bool connect(const std::string& connect_string) override
	{
		auto config = core::connection_config::from_string(connect_string);
		auto result = backend_->initialize(config);
		return result.is_ok();
	}

	/**
	 * @brief Create/prepare a query
	 * @param query_string The SQL query to prepare
	 * @return true on success, false on failure
	 *
	 * @note The underlying database_backend doesn't have a direct equivalent.
	 *       This method executes the query using execute_query().
	 */
	bool create_query(const std::string& query_string) override
	{
		auto result = backend_->execute_query(query_string);
		return result.is_ok();
	}

	/**
	 * @brief Execute an INSERT query
	 * @param query_string The SQL INSERT statement
	 * @return Number of rows inserted, or 0 on failure
	 */
	unsigned int insert_query(const std::string& query_string) override
	{
		auto result = backend_->insert_query(query_string);
		if (result.is_ok())
		{
			return static_cast<unsigned int>(result.value());
		}
		return 0;
	}

	/**
	 * @brief Execute an UPDATE query
	 * @param query_string The SQL UPDATE statement
	 * @return Number of rows updated, or 0 on failure
	 */
	unsigned int update_query(const std::string& query_string) override
	{
		auto result = backend_->update_query(query_string);
		if (result.is_ok())
		{
			return static_cast<unsigned int>(result.value());
		}
		return 0;
	}

	/**
	 * @brief Execute a DELETE query
	 * @param query_string The SQL DELETE statement
	 * @return Number of rows deleted, or 0 on failure
	 */
	unsigned int delete_query(const std::string& query_string) override
	{
		auto result = backend_->delete_query(query_string);
		if (result.is_ok())
		{
			return static_cast<unsigned int>(result.value());
		}
		return 0;
	}

	/**
	 * @brief Execute a SELECT query
	 * @param query_string The SQL SELECT statement
	 * @return Query results, or empty vector on failure
	 */
	database_result select_query(const std::string& query_string) override
	{
		auto result = backend_->select_query(query_string);
		if (result.is_ok())
		{
			// Convert from core::database_result to database::database_result
			const auto& core_result = result.value();
			database_result legacy_result;
			legacy_result.reserve(core_result.size());

			for (const auto& row : core_result)
			{
				database_row legacy_row;
				for (const auto& [key, value] : row)
				{
					legacy_row[key] = value;
				}
				legacy_result.push_back(std::move(legacy_row));
			}
			return legacy_result;
		}
		return {};
	}

	/**
	 * @brief Execute a general SQL query
	 * @param query_string The SQL statement
	 * @return true on success, false on failure
	 */
	bool execute_query(const std::string& query_string) override
	{
		auto result = backend_->execute_query(query_string);
		return result.is_ok();
	}

	/**
	 * @brief Disconnect from the database
	 * @return true on success, false on failure
	 */
	bool disconnect() override
	{
		auto result = backend_->shutdown();
		return result.is_ok();
	}

	/**
	 * @brief Get the underlying database_backend
	 * @return Const reference to the wrapped backend
	 *
	 * Use this to access backend-specific features not available
	 * through the database_base interface.
	 */
	const core::database_backend& backend() const
	{
		return *backend_;
	}

	/**
	 * @brief Get the underlying database_backend (non-const)
	 * @return Reference to the wrapped backend
	 */
	core::database_backend& backend()
	{
		return *backend_;
	}

private:
	std::unique_ptr<core::database_backend> backend_;
};

} // namespace database

// Restore diagnostic settings
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
