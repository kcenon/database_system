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
 * @file concepts.h
 * @brief C++20 concepts for database_system type validation.
 *
 * This header provides C++20 concepts for compile-time type validation
 * specific to database operations. It re-exports common_system concepts
 * and adds database-specific concepts.
 *
 * Available concept categories:
 * - Callable concepts: From common_system (Invocable, VoidCallable, etc.)
 * - Database concepts: QueryCallback, ConnectionFactory, etc.
 *
 * Requirements:
 * - C++20 compiler with concepts support
 * - GCC 10+, Clang 10+, MSVC 2019 16.3+
 *
 * Benefits of using concepts:
 * - **Clearer error messages**: Template errors are displayed as concept
 *   violations instead of hundreds of lines of SFINAE failures
 * - **Self-documenting code**: Concepts express type requirements explicitly
 * - **Better IDE support**: More accurate auto-completion and type hints
 * - **Code simplification**: Eliminates std::enable_if boilerplate
 *
 * Example usage:
 * @code
 * #include "database/core/concepts.h"
 *
 * using namespace database::concepts;
 *
 * // Use callable concepts for async operations
 * template<Invocable F>
 * auto submit_task(F&& func) {
 *     return executor.submit(std::forward<F>(func));
 * }
 *
 * // Use database-specific concepts
 * template<QueryCallback<database_result> F>
 * void on_query_complete(F&& callback) {
 *     // callback will be invoked with database_result
 * }
 * @endcode
 */

#pragma once

#include <concepts>
#include <type_traits>
#include <functional>
#include <memory>
#include <string>
#include <future>

// Forward declarations
namespace database {
class database_base;
namespace core {
class database_backend;
// database_row is a type alias defined in database_backend.h, not forward-declarable
} // namespace core
} // namespace database

namespace database::concepts {

// ═══════════════════════════════════════════════════════════════════════════
// Callable Concepts (adapted from common_system for database_system)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @concept Invocable
 * @brief A callable type that can be invoked with given arguments.
 */
template<typename F, typename... Args>
concept Invocable = std::invocable<F, Args...>;

/**
 * @concept VoidCallable
 * @brief A callable type that returns void when invoked.
 */
template<typename F, typename... Args>
concept VoidCallable = Invocable<F, Args...> &&
    std::is_void_v<std::invoke_result_t<F, Args...>>;

/**
 * @concept ReturnsResult
 * @brief A callable type that returns a value convertible to the specified type.
 */
template<typename F, typename R, typename... Args>
concept ReturnsResult = Invocable<F, Args...> &&
    std::convertible_to<std::invoke_result_t<F, Args...>, R>;

/**
 * @concept Predicate
 * @brief A callable type that returns a boolean value.
 */
template<typename F, typename... Args>
concept Predicate = Invocable<F, Args...> &&
    std::convertible_to<std::invoke_result_t<F, Args...>, bool>;

/**
 * @concept NoexceptCallable
 * @brief A callable type that is marked noexcept.
 */
template<typename F, typename... Args>
concept NoexceptCallable = Invocable<F, Args...> &&
    std::is_nothrow_invocable_v<F, Args...>;

/**
 * @concept DelayedCallable
 * @brief A callable suitable for delayed execution.
 */
template<typename F>
concept DelayedCallable = VoidCallable<F> &&
    std::move_constructible<std::decay_t<F>>;

/**
 * @concept AsyncCallable
 * @brief A callable suitable for async execution.
 */
template<typename F, typename R>
concept AsyncCallable = Invocable<F> &&
    std::same_as<std::invoke_result_t<F>, R>;

// ═══════════════════════════════════════════════════════════════════════════
// Database-Specific Callable Concepts
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @concept QueryCallback
 * @brief A callable that handles query results.
 *
 * Use this concept for callbacks that process database query results.
 *
 * Example usage:
 * @code
 * template<QueryCallback<database_result> F>
 * void execute_and_callback(const std::string& query, F&& callback) {
 *     auto result = db->select(query);
 *     callback(std::move(result));
 * }
 * @endcode
 */
template<typename F, typename ResultType>
concept QueryCallback = Invocable<F, ResultType>;

/**
 * @concept ErrorHandler
 * @brief A callable that handles database errors.
 *
 * Example usage:
 * @code
 * template<ErrorHandler F>
 * void set_error_handler(F&& handler) {
 *     error_handler_ = std::forward<F>(handler);
 * }
 * @endcode
 */
template<typename F>
concept ErrorHandler = Invocable<F, const std::exception&>;

/**
 * @concept ConnectionFactory
 * @brief A callable that creates database connections.
 *
 * Example usage:
 * @code
 * template<ConnectionFactory F>
 * void set_connection_factory(F&& factory) {
 *     factory_ = std::forward<F>(factory);
 * }
 * @endcode
 */
template<typename F>
concept ConnectionFactory = Invocable<F> &&
    std::convertible_to<std::invoke_result_t<F>, std::unique_ptr<database_base>>;

/**
 * @concept BackendFactory
 * @brief A callable that creates database backends.
 *
 * Example usage:
 * @code
 * template<BackendFactory F>
 * void register_backend(const std::string& name, F&& factory) {
 *     backends_[name] = std::forward<F>(factory);
 * }
 * @endcode
 */
template<typename F>
concept BackendFactory = Invocable<F> &&
    std::convertible_to<std::invoke_result_t<F>, std::unique_ptr<core::database_backend>>;

// ═══════════════════════════════════════════════════════════════════════════
// Event and Stream Concepts
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @concept StreamEventHandler
 * @brief A callable that handles stream events.
 *
 * Example usage:
 * @code
 * template<StreamEventHandler<stream_event> F>
 * void register_handler(const std::string& channel, F&& handler) {
 *     handlers_[channel] = std::forward<F>(handler);
 * }
 * @endcode
 */
template<typename F, typename EventType>
concept StreamEventHandler = VoidCallable<F, const EventType&>;

/**
 * @concept StreamEventFilter
 * @brief A callable that filters stream events.
 *
 * Example usage:
 * @code
 * template<StreamEventFilter<stream_event> F>
 * void add_filter(const std::string& channel, F&& filter) {
 *     filters_[channel] = std::forward<F>(filter);
 * }
 * @endcode
 */
template<typename F, typename EventType>
concept StreamEventFilter = Predicate<F, const EventType&>;

// ═══════════════════════════════════════════════════════════════════════════
// Transaction Concepts
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @concept TransactionAction
 * @brief A callable that represents a transaction action.
 *
 * Example usage:
 * @code
 * template<TransactionAction F>
 * void add_step(F&& action) {
 *     steps_.push_back(std::forward<F>(action));
 * }
 * @endcode
 */
template<typename F>
concept TransactionAction = Invocable<F>;

/**
 * @concept CompensationAction
 * @brief A callable that represents a compensation (rollback) action.
 *
 * Example usage:
 * @code
 * template<TransactionAction A, CompensationAction C>
 * void add_saga_step(A&& action, C&& compensation) {
 *     steps_.emplace_back(std::forward<A>(action), std::forward<C>(compensation));
 * }
 * @endcode
 */
template<typename F>
concept CompensationAction = Invocable<F>;

// ═══════════════════════════════════════════════════════════════════════════
// Pool Concepts
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @concept PooledResource
 * @brief A type that can be managed by a pool.
 *
 * Types satisfying this concept can be pooled and reused.
 *
 * Example usage:
 * @code
 * template<PooledResource T>
 * class resource_pool {
 *     std::shared_ptr<T> acquire();
 *     void release(std::shared_ptr<T> resource);
 * };
 * @endcode
 */
template<typename T>
concept PooledResource = std::is_class_v<T> &&
    std::is_default_constructible_v<T>;

/**
 * @concept ConnectionWrapper
 * @brief A type that wraps a database connection.
 *
 * Example usage:
 * @code
 * template<ConnectionWrapper W>
 * void use_connection(W& wrapper) {
 *     auto* conn = wrapper.get();
 *     conn->execute("SELECT 1");
 * }
 * @endcode
 */
template<typename T>
concept ConnectionWrapper = requires(T t) {
    { t.get() } -> std::convertible_to<database_base*>;
    { t.is_valid() } -> std::convertible_to<bool>;
};

// ═══════════════════════════════════════════════════════════════════════════
// Task Execution Concepts
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @concept SubmittableTask
 * @brief A callable suitable for submission to an async executor.
 *
 * This concept combines invocability with move-constructibility,
 * ensuring the callable can be stored and executed asynchronously.
 *
 * Example usage:
 * @code
 * template<SubmittableTask F, typename... Args>
 * auto submit(F&& func, Args&&... args) {
 *     using return_type = std::invoke_result_t<F, Args...>;
 *     // ... create packaged_task and submit
 * }
 * @endcode
 */
template<typename F, typename... Args>
concept SubmittableTask = Invocable<F, Args...> &&
    std::move_constructible<std::decay_t<F>>;

/**
 * @concept VoidTask
 * @brief A callable that returns void, suitable for fire-and-forget execution.
 *
 * Example usage:
 * @code
 * template<VoidTask F>
 * void execute(F&& func) {
 *     func();
 * }
 * @endcode
 */
template<typename F, typename... Args>
concept VoidTask = VoidCallable<F, Args...> &&
    std::move_constructible<std::decay_t<F>>;

} // namespace database::concepts
