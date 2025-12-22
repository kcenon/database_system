// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file common_system_database_adapter.h
 * @brief Adapter to bridge database_system's database_manager with common_system's IDatabase.
 *
 * This adapter implements the common::interfaces::IDatabase interface using
 * database_system's database_manager as the underlying implementation.
 *
 * @see TICKET-103 for integration requirements.
 */

#pragma once

#include <memory>
#include <string>

#include "../config/feature_flags.h"

#if KCENON_HAS_COMMON_SYSTEM

#include <kcenon/common/interfaces/database_interface.h>
#include <kcenon/common/patterns/result.h>

// Include database_system headers
#include "../../database/database_manager.h"
#include "../../database/core/database_context.h"

namespace kcenon::database::adapters {

/**
 * @class common_system_database_adapter
 * @brief Adapter that implements IDatabase interface using database_manager
 *
 * This adapter bridges the gap between common_system's IDatabase interface
 * and database_system's database_manager implementation.
 *
 * Thread Safety:
 * - Delegates thread safety to underlying database_manager
 * - Each adapter instance should be used from a single thread
 *   or protected with appropriate synchronization
 */
class common_system_database_adapter : public common::interfaces::IDatabase {
public:
    /**
     * @brief Construct adapter with a new database_manager instance
     * @param db_type The database type to use (defaults to PostgreSQL)
     */
    explicit common_system_database_adapter(
        ::database::database_types db_type = ::database::database_types::postgresql)
        : context_(std::make_shared<::database::database_context>())
        , manager_(std::make_shared<::database::database_manager>(context_))
        , db_type_(db_type)
        , connected_(false) {
        manager_->set_mode(db_type_);
    }

    /**
     * @brief Construct adapter with an existing database_manager
     * @param manager Pre-configured database_manager instance
     */
    explicit common_system_database_adapter(
        std::shared_ptr<::database::database_manager> manager)
        : manager_(std::move(manager))
        , db_type_(manager_ ? manager_->database_type() : ::database::database_types::postgresql)
        , connected_(false) {}

    ~common_system_database_adapter() override {
        if (connected_) {
            disconnect();
        }
    }

    // Non-copyable
    common_system_database_adapter(const common_system_database_adapter&) = delete;
    common_system_database_adapter& operator=(const common_system_database_adapter&) = delete;

    // Movable
    common_system_database_adapter(common_system_database_adapter&&) = default;
    common_system_database_adapter& operator=(common_system_database_adapter&&) = default;

    /**
     * @brief Connect to database using connection string
     * @param connection_string Database-specific connection string
     * @return VoidResult indicating success or error
     */
    common::VoidResult connect(const std::string& connection_string) override {
        if (!manager_) {
            return common::make_error<std::monostate>(
                1, "Database manager not initialized", "database_system::adapter");
        }

        if (connected_) {
            return common::make_error<std::monostate>(
                2, "Already connected to database", "database_system::adapter");
        }

        if (manager_->connect(connection_string)) {
            connected_ = true;
            return common::VoidResult::ok({});
        }

        return common::make_error<std::monostate>(
            3, "Failed to connect to database", "database_system::adapter");
    }

    /**
     * @brief Disconnect from database
     * @return VoidResult indicating success or error
     */
    common::VoidResult disconnect() override {
        if (!manager_) {
            return common::make_error<std::monostate>(
                1, "Database manager not initialized", "database_system::adapter");
        }

        if (!connected_) {
            return common::VoidResult::ok({});  // Already disconnected
        }

        if (manager_->disconnect()) {
            connected_ = false;
            return common::VoidResult::ok({});
        }

        return common::make_error<std::monostate>(
            4, "Failed to disconnect from database", "database_system::adapter");
    }

    /**
     * @brief Execute a query and return results
     * @param query SQL query string
     * @return Result containing query results or error
     */
    common::Result<common::database_result> execute_query(const std::string& query) override {
        if (!manager_) {
            return common::make_error<common::database_result>(
                1, "Database manager not initialized", "database_system::adapter");
        }

        if (!connected_) {
            return common::make_error<common::database_result>(
                5, "Not connected to database", "database_system::adapter");
        }

        auto result = manager_->select_query(query);

        // Convert database::database_result to common::database_result
        common::database_result common_result;
        common_result.reserve(result.size());

        for (const auto& row : result) {
            common::database_row common_row;
            for (const auto& [key, value] : row) {
                common_row[key] = convert_value(value);
            }
            common_result.push_back(std::move(common_row));
        }

        return common::Result<common::database_result>::ok(std::move(common_result));
    }

    /**
     * @brief Execute a command without returning results
     * @param command SQL command string (INSERT, UPDATE, DELETE, etc.)
     * @return VoidResult indicating success or error
     */
    common::VoidResult execute_command(const std::string& command) override {
        if (!manager_) {
            return common::make_error<std::monostate>(
                1, "Database manager not initialized", "database_system::adapter");
        }

        if (!connected_) {
            return common::make_error<std::monostate>(
                5, "Not connected to database", "database_system::adapter");
        }

        // Determine command type and execute appropriate method
        std::string upper_command = command.substr(0, 10);
        std::transform(upper_command.begin(), upper_command.end(),
                       upper_command.begin(), ::toupper);

        unsigned int affected_rows = 0;
        if (upper_command.find("INSERT") != std::string::npos) {
            affected_rows = manager_->insert_query(command);
        } else if (upper_command.find("UPDATE") != std::string::npos) {
            affected_rows = manager_->update_query(command);
        } else if (upper_command.find("DELETE") != std::string::npos) {
            affected_rows = manager_->delete_query(command);
        } else {
            // For DDL or other commands, use create_query
            if (!manager_->create_query(command)) {
                return common::make_error<std::monostate>(
                    6, "Failed to execute command", "database_system::adapter");
            }
            return common::VoidResult::ok({});
        }

        // affected_rows of 0 might be valid (e.g., no matching rows)
        return common::VoidResult::ok({});
    }

    /**
     * @brief Begin a database transaction
     * @return VoidResult indicating success or error
     */
    common::VoidResult begin_transaction() override {
        return execute_command("BEGIN TRANSACTION");
    }

    /**
     * @brief Commit the current transaction
     * @return VoidResult indicating success or error
     */
    common::VoidResult commit() override {
        return execute_command("COMMIT");
    }

    /**
     * @brief Rollback the current transaction
     * @return VoidResult indicating success or error
     */
    common::VoidResult rollback() override {
        return execute_command("ROLLBACK");
    }

    /**
     * @brief Check if database is currently connected
     * @return true if connected, false otherwise
     */
    bool is_connected() const override {
        return connected_;
    }

    /**
     * @brief Get the underlying database_manager
     * @return Shared pointer to the database_manager
     */
    std::shared_ptr<::database::database_manager> get_manager() const {
        return manager_;
    }

    /**
     * @brief Get the database context
     * @return Shared pointer to the database_context
     */
    std::shared_ptr<::database::database_context> get_context() const {
        return context_;
    }

private:
    /**
     * @brief Convert database_system value to common_system value
     */
    static common::database_value convert_value(const ::database::database_value& value) {
        return std::visit([](auto&& arg) -> common::database_value {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ::database::database_null>) {
                return common::database_null{};
            } else if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return arg;
            } else if constexpr (std::is_same_v<T, double>) {
                return arg;
            } else if constexpr (std::is_same_v<T, bool>) {
                return arg;
            } else {
                return common::database_null{};
            }
        }, value);
    }

    std::shared_ptr<::database::database_context> context_;
    std::shared_ptr<::database::database_manager> manager_;
    ::database::database_types db_type_;
    bool connected_;
};

} // namespace kcenon::database::adapters

#endif // KCENON_HAS_COMMON_SYSTEM
