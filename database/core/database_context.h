#pragma once

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

namespace database
{

// Forward declarations
class connection_pool_base;
class connection_pool_manager;
struct connection_pool_config;
enum class database_types : uint8_t;

namespace monitoring {
    class performance_monitor;
    class connection_leak_detector;
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
        return pool_manager_ != nullptr;
    }

    /**
     * @brief Get connection pool manager instance (inline for zero overhead)
     * @return Shared pointer to connection pool manager
     *
     * @details Returns the connection pool manager for this context. This manages
     * connection pools for different database types.
     *
     * @note This method is lock-free, inline, and noexcept for maximum performance.
     * The pool_manager_ is set once during construction and never modified,
     * making it safe to read without locks.
     *
     * @since Sprint 2 (Task 2.3)
     */
    inline std::shared_ptr<connection_pool_manager> get_pool_manager() const noexcept {
        // Lock-free read: pool_manager_ is immutable after construction
        // std::shared_ptr copy is thread-safe due to atomic ref counting
        // Inline for zero call overhead in hot paths
        return pool_manager_;
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
     * @brief Get connection leak detector instance
     * @return Shared pointer to connection leak detector
     *
     * @details Returns the leak detector for monitoring connection leases.
     *
     * @note Lock-free read, inline for performance.
     * @since Sprint 3 (Task 3.2)
     */
    inline std::shared_ptr<monitoring::connection_leak_detector> get_leak_detector() const noexcept {
        return leak_detector_;
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
    /// Connection pool manager instance (Sprint 2, Task 2.3)
    std::shared_ptr<connection_pool_manager> pool_manager_;

    /// Performance monitor instance (Sprint 3, Task 3.2)
    std::shared_ptr<monitoring::performance_monitor> performance_monitor_;

    /// Connection leak detector instance (Sprint 3, Task 3.2)
    std::shared_ptr<monitoring::connection_leak_detector> leak_detector_;

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

} // namespace database
