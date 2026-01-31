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

#include <atomic>
#include <memory>
#include <mutex>
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
 * - is_connected() is thread-safe (uses std::atomic<bool>)
 * - connect()/disconnect() are serialized by connection_mutex_
 * - execute_query()/execute_command() thread-safety delegated to database_manager
 * - Multiple threads may safely check connection state concurrently
 * - Connection state transitions are protected against race conditions
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
        if (connected_.load(std::memory_order_acquire)) {
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
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (!manager_) {
            return common::make_error<std::monostate>(
                ERROR_MANAGER_NOT_INITIALIZED, "Database manager not initialized", "database_system::adapter");
        }

        if (connected_.load(std::memory_order_acquire)) {
            return common::make_error<std::monostate>(
                ERROR_ALREADY_CONNECTED, "Already connected to database", "database_system::adapter");
        }

        auto result = manager_->connect_result(connection_string);
        if (result.is_ok()) {
            connected_.store(true, std::memory_order_release);
            return common::VoidResult::ok({});
        }

        return common::make_error<std::monostate>(
            ERROR_CONNECTION_FAILED, result.error().message, "database_system::adapter");
    }

    /**
     * @brief Disconnect from database
     * @return VoidResult indicating success or error
     */
    common::VoidResult disconnect() override {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (!manager_) {
            return common::make_error<std::monostate>(
                ERROR_MANAGER_NOT_INITIALIZED, "Database manager not initialized", "database_system::adapter");
        }

        if (!connected_.load(std::memory_order_acquire)) {
            return common::VoidResult::ok({});  // Already disconnected
        }

        auto result = manager_->disconnect_result();
        if (result.is_ok()) {
            connected_.store(false, std::memory_order_release);
            return common::VoidResult::ok({});
        }

        return common::make_error<std::monostate>(
            ERROR_DISCONNECT_FAILED, result.error().message, "database_system::adapter");
    }

    /**
     * @brief Execute a query and return results
     * @param query SQL query string
     * @return Result containing query results or error
     */
    common::Result<common::database_result> execute_query(const std::string& query) override {
        if (!manager_) {
            return common::make_error<common::database_result>(
                ERROR_MANAGER_NOT_INITIALIZED, "Database manager not initialized", "database_system::adapter");
        }

        if (!connected_.load(std::memory_order_acquire)) {
            return common::make_error<common::database_result>(
                ERROR_NOT_CONNECTED, "Not connected to database", "database_system::adapter");
        }

        auto result = manager_->select_query_result(query);
        if (!result.is_ok()) {
            return common::make_error<common::database_result>(
                ERROR_QUERY_FAILED, result.error().message, "database_system::adapter");
        }

        // Convert core::database_result to common::database_result
        common::database_result common_result;
        const auto& core_result = result.value();
        common_result.reserve(core_result.size());

        for (const auto& row : core_result) {
            common::database_row common_row;
            for (const auto& [key, value] : row) {
                common_row[key] = convert_core_value(value);
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
                ERROR_MANAGER_NOT_INITIALIZED, "Database manager not initialized", "database_system::adapter");
        }

        if (!connected_.load(std::memory_order_acquire)) {
            return common::make_error<std::monostate>(
                ERROR_NOT_CONNECTED, "Not connected to database", "database_system::adapter");
        }

        // Determine command type and execute appropriate method
        std::string upper_command = command.substr(0, 10);
        std::transform(upper_command.begin(), upper_command.end(),
                       upper_command.begin(), ::toupper);

        if (upper_command.find("INSERT") != std::string::npos) {
            auto result = manager_->insert_query_result(command);
            if (!result.is_ok()) {
                return common::make_error<std::monostate>(
                    ERROR_QUERY_FAILED, result.error().message, "database_system::adapter");
            }
        } else if (upper_command.find("UPDATE") != std::string::npos) {
            auto result = manager_->update_query_result(command);
            if (!result.is_ok()) {
                return common::make_error<std::monostate>(
                    ERROR_QUERY_FAILED, result.error().message, "database_system::adapter");
            }
        } else if (upper_command.find("DELETE") != std::string::npos) {
            auto result = manager_->delete_query_result(command);
            if (!result.is_ok()) {
                return common::make_error<std::monostate>(
                    ERROR_QUERY_FAILED, result.error().message, "database_system::adapter");
            }
        } else {
            // For DDL or other commands, use create_query_result
            auto result = manager_->create_query_result(command);
            if (!result.is_ok()) {
                return common::make_error<std::monostate>(
                    ERROR_QUERY_FAILED, result.error().message, "database_system::adapter");
            }
        }

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
        return connected_.load(std::memory_order_acquire);
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
    // Error codes for database adapter
    // Using database_system range: -500 to -599
    static constexpr int ERROR_MANAGER_NOT_INITIALIZED = -580;
    static constexpr int ERROR_ALREADY_CONNECTED = -581;
    static constexpr int ERROR_CONNECTION_FAILED = -582;
    static constexpr int ERROR_DISCONNECT_FAILED = -583;
    static constexpr int ERROR_NOT_CONNECTED = -584;
    static constexpr int ERROR_QUERY_FAILED = -585;

    /**
     * @brief Convert core::database_value to common_system value
     */
    static common::database_value convert_core_value(const ::database::core::database_value& value) {
        return std::visit([](auto&& arg) -> common::database_value {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
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
    mutable std::mutex connection_mutex_;
    std::atomic<bool> connected_;
};

} // namespace kcenon::database::adapters

#endif // KCENON_HAS_COMMON_SYSTEM
