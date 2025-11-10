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
struct connection_pool_config;
enum class database_types : uint8_t;

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
    bool is_initialized() const {
        return true; // For now, always initialized
    }

private:
    // Future Sprint 3: Add component members here
    // std::shared_ptr<connection_pool_manager_interface> pool_manager_;
    // std::shared_ptr<performance_monitor_interface> perf_monitor_;
    // std::shared_ptr<credential_manager_interface> credential_mgr_;

    /// Mutex for thread-safe access
    mutable std::mutex mutex_;
};

} // namespace database
