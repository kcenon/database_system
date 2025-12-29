// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file service_registration.h
 * @brief Service container registration for database_system services.
 *
 * This header provides functions to register database_system services
 * with the unified service container from common_system.
 *
 * @see TICKET-103 for integration requirements.
 */

#pragma once

#include <memory>
#include <string>

#include "../config/feature_flags.h"

#if KCENON_HAS_COMMON_SYSTEM

#include <kcenon/common/di/service_container.h>
#include <kcenon/common/interfaces/database_interface.h>

#include "../adapters/common_system_database_adapter.h"
#include "../../database/database_manager.h"
#include "../../database/core/database_context.h"

namespace kcenon::database::di {

/**
 * @brief Configuration for database service registration
 */
struct database_registration_config {
    /// Database type (postgresql, mysql, sqlite, etc.)
    ::database::database_types db_type = ::database::database_types::postgresql;

    /// Connection string (optional - can be set later via connect())
    std::string connection_string;

    /// Whether to connect immediately upon registration
    bool connect_on_register = false;

    /// Service lifetime (typically singleton for database connections)
    common::di::service_lifetime lifetime = common::di::service_lifetime::singleton;
};

/**
 * @brief Register database services with the service container.
 *
 * Registers IDatabase implementation using database_system's database_manager.
 * By default, registers as a singleton with PostgreSQL backend.
 *
 * @param container The service container to register with
 * @param config Optional configuration for the database
 * @return VoidResult indicating success or registration error
 *
 * @code
 * auto& container = common::di::service_container::global();
 *
 * // Register with default configuration
 * auto result = register_database_services(container);
 *
 * // Or with custom configuration
 * database_registration_config config;
 * config.db_type = ::database::database_types::sqlite;
 * config.connection_string = "database.db";
 * config.connect_on_register = true;
 * auto result = register_database_services(container, config);
 *
 * // Then resolve database anywhere in the application
 * auto db = container.resolve<common::interfaces::IDatabase>().value();
 * db->connect("host=localhost dbname=mydb");
 * auto result = db->execute_query("SELECT * FROM users");
 * @endcode
 */
inline common::VoidResult register_database_services(
    common::di::IServiceContainer& container,
    const database_registration_config& config = {}) {

    // Check if already registered
    if (container.is_registered<common::interfaces::IDatabase>()) {
        return common::make_error<std::monostate>(
            common::di::di_error_codes::already_registered,
            "IDatabase is already registered",
            "database_system::di"
        );
    }

    // Register database factory
    return container.register_factory<common::interfaces::IDatabase>(
        [config](common::di::IServiceContainer&) -> std::shared_ptr<common::interfaces::IDatabase> {
            // Create adapter with configured database type
            auto adapter = std::make_shared<adapters::common_system_database_adapter>(config.db_type);

            // Connect if connection string provided and connect_on_register is true
            if (config.connect_on_register && !config.connection_string.empty()) {
                auto result = adapter->connect(config.connection_string);
                if (result.is_err()) {
                    // Return adapter anyway - caller can check is_connected()
                    // This is intentional to avoid throwing in factory
                }
            }

            return adapter;
        },
        config.lifetime
    );
}

/**
 * @brief Register a pre-configured database_manager instance.
 *
 * Use this when you have already created a database_manager instance and want
 * to register it with the container.
 *
 * @param container The service container to register with
 * @param manager The database_manager instance to register
 * @return VoidResult indicating success or registration error
 *
 * @code
 * // Create database_manager manually
 * auto context = std::make_shared<::database::database_context>();
 * auto manager = std::make_shared<::database::database_manager>(context);
 * manager->set_mode(::database::database_types::postgresql);
 * manager->connect("host=localhost dbname=mydb");
 *
 * // Register the instance
 * register_database_instance(container, manager);
 * @endcode
 */
inline common::VoidResult register_database_instance(
    common::di::IServiceContainer& container,
    std::shared_ptr<::database::database_manager> manager) {

    if (!manager) {
        return common::make_error<std::monostate>(
            common::error_codes::INVALID_ARGUMENT,
            "Cannot register null database_manager instance",
            "database_system::di"
        );
    }

    auto adapter = std::make_shared<adapters::common_system_database_adapter>(std::move(manager));

    return container.register_instance<common::interfaces::IDatabase>(adapter);
}

/**
 * @brief Unregister database services from the container.
 *
 * @param container The service container to unregister from
 * @return VoidResult indicating success or error
 */
inline common::VoidResult unregister_database_services(
    common::di::IServiceContainer& container) {

    return container.unregister<common::interfaces::IDatabase>();
}

/**
 * @brief Get the underlying database_manager from an IDatabase resolved from the container.
 *
 * This utility function allows accessing the underlying database_manager
 * when needed for advanced operations like connection pooling.
 *
 * @param database The IDatabase instance (should be a common_system_database_adapter)
 * @return Shared pointer to the underlying database_manager, or nullptr if not an adapter
 *
 * @code
 * auto idatabase = container.resolve<common::interfaces::IDatabase>().value();
 * auto manager = get_underlying_database_manager(idatabase);
 * if (manager) {
 *     // Use advanced database_manager features (e.g., ProxyMode configuration)
 *     auto connected = manager->is_connected();
 * }
 * @endcode
 */
inline std::shared_ptr<::database::database_manager> get_underlying_database_manager(
    const std::shared_ptr<common::interfaces::IDatabase>& database) {

    auto adapter = std::dynamic_pointer_cast<adapters::common_system_database_adapter>(database);
    if (adapter) {
        return adapter->get_manager();
    }
    return nullptr;
}

/**
 * @brief Register all database_system services with the container.
 *
 * Convenience function to register all available database_system services.
 *
 * @param container The service container to register with
 * @param config Optional configuration for database
 * @return VoidResult indicating success or registration error
 */
inline common::VoidResult register_all_database_services(
    common::di::IServiceContainer& container,
    const database_registration_config& config = {}) {

    // Register IDatabase
    auto result = register_database_services(container, config);
    if (result.is_err()) {
        return result;
    }

    return common::VoidResult::ok({});
}

} // namespace kcenon::database::di

#endif // KCENON_HAS_COMMON_SYSTEM
