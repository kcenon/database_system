// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <memory>
#include <mutex>
#include <cstdint>

/**
 * @file database_context.h
 * @brief Dependency injection container for database system components
 * @author kcenon
 * @since 1.0.0 (Sprint 2)
 *
 * @details This context replaces global singletons with injected dependencies,
 * enabling better testability and supporting multiple independent database instances.
 *
 * Key Features:
 * - Dependency injection for connection pool manager and other components
 * - Support for mock injection in tests
 * - Thread-safe component management
 * - Multiple independent contexts possible (no global state)
 *
 * @example
 * @code
 * // Production usage
 * auto context = std::make_shared<database_context>();
 * auto db_mgr = std::make_shared<database_manager>(context);
 *
 * // Test usage with mocks
 * auto mock_pool_mgr = std::make_shared<mock_connection_pool_manager>();
 * auto context = std::make_shared<database_context>(mock_pool_mgr);
 * auto db_mgr = std::make_shared<database_manager>(context);
 * @endcode
 */

namespace kcenon::database
{

// Forward declarations
enum class database_types : uint8_t;

namespace monitoring {
    class performance_monitor;
}

namespace orm {
    class entity_manager;
}

namespace async {
    class transaction_coordinator;
}

namespace security {
    class credential_manager;
    class access_control;
    class audit_logger;
    class security_monitor;
    class encryption_manager;
}

/**
 * @class database_context
 * @brief Dependency injection container for database components
 *
 * @details This class manages the lifetime and dependencies of database components.
 * It replaces singleton pattern with dependency injection, enabling:
 * - Testing with mock objects
 * - Multiple independent database instances
 * - Better separation of concerns
 * - No hidden global state
 *
 * Thread Safety: All methods are thread-safe. Component access is protected
 * by internal synchronization.
 */
class database_context {
public:
    /**
     * @brief Default constructor - creates default implementations
     *
     * @details Creates a context with default implementations:
     * - Connection pool management (Sprint 3)
     * - Performance monitoring (Sprint 3)
     * - Other components as needed (Sprint 3+)
     *
     * @note For custom/mock implementations, additional constructors will be added
     * in Sprint 3 when implementing other singleton conversions.
     */
    database_context();

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~database_context();

    // Delete copy and move to prevent accidental sharing
    database_context(const database_context&) = delete;
    database_context& operator=(const database_context&) = delete;
    database_context(database_context&&) = delete;
    database_context& operator=(database_context&&) = delete;

    /**
     * @brief Check if context is initialized
     * @return true if all required components are initialized
     */
    inline bool is_initialized() const noexcept {
        return performance_monitor_ != nullptr;
    }

    /**
     * @brief Get performance monitor instance
     * @return Shared pointer to performance monitor
     *
     * @details Returns the performance monitor for tracking query metrics,
     * connection usage, and system performance.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.2)
     */
    inline std::shared_ptr<monitoring::performance_monitor> get_performance_monitor() const noexcept {
        return performance_monitor_;
    }

    /**
     * @brief Get entity manager instance
     * @return Shared pointer to entity manager
     *
     * @details Returns the ORM entity manager for entity metadata and query building.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.1)
     */
    inline std::shared_ptr<orm::entity_manager> get_entity_manager() const noexcept {
        return entity_manager_;
    }

    /**
     * @brief Get transaction coordinator instance
     * @return Shared pointer to transaction coordinator
     *
     * @details Returns the transaction coordinator for distributed transaction management.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.1)
     */
    inline std::shared_ptr<async::transaction_coordinator> get_transaction_coordinator() const noexcept {
        return transaction_coordinator_;
    }

    /**
     * @brief Get credential manager instance
     * @return Shared pointer to credential manager
     *
     * @details Returns the credential manager for encrypted credential storage.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.3)
     */
    inline std::shared_ptr<security::credential_manager> get_credential_manager() const noexcept {
        return credential_manager_;
    }

    /**
     * @brief Get access control instance
     * @return Shared pointer to access control
     *
     * @details Returns the access control for RBAC (Role-Based Access Control).
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.3)
     */
    inline std::shared_ptr<security::access_control> get_access_control() const noexcept {
        return access_control_;
    }

    /**
     * @brief Get audit logger instance
     * @return Shared pointer to audit logger
     *
     * @details Returns the audit logger for security event logging.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.3)
     */
    inline std::shared_ptr<security::audit_logger> get_audit_logger() const noexcept {
        return audit_logger_;
    }

    /**
     * @brief Get security monitor instance
     * @return Shared pointer to security monitor
     *
     * @details Returns the security monitor for threat detection and alerting.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.3)
     */
    inline std::shared_ptr<security::security_monitor> get_security_monitor() const noexcept {
        return security_monitor_;
    }

    /**
     * @brief Get encryption manager instance
     * @return Shared pointer to encryption manager
     *
     * @details Returns the encryption manager for data encryption and key management.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.3)
     */
    inline std::shared_ptr<security::encryption_manager> get_encryption_manager() const noexcept {
        return encryption_manager_;
    }

private:
    /// Performance monitor instance (Sprint 3, Task 3.2)
    std::shared_ptr<monitoring::performance_monitor> performance_monitor_;

    /// Entity manager instance (Sprint 3, Task 3.1)
    std::shared_ptr<orm::entity_manager> entity_manager_;

    /// Transaction coordinator instance (Sprint 3, Task 3.1)
    std::shared_ptr<async::transaction_coordinator> transaction_coordinator_;

    /// Security component instances (Sprint 3, Task 3.3)
    std::shared_ptr<security::credential_manager> credential_manager_;
    std::shared_ptr<security::access_control> access_control_;
    std::shared_ptr<security::audit_logger> audit_logger_;
    std::shared_ptr<security::security_monitor> security_monitor_;
    std::shared_ptr<security::encryption_manager> encryption_manager_;

    /// Mutex for thread-safe access
    mutable std::mutex mutex_;
};

} // namespace kcenon::database
