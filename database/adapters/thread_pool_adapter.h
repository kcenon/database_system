// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file thread_pool_adapter.h
 * @brief Adapter layer for thread_system integration
 *
 * This file provides conditional compilation support for thread_system.
 * When USE_THREAD_SYSTEM is defined, it uses high-performance thread_system.
 * Otherwise, it falls back to standard library threading.
 *
 * ### Design Pattern: Adapter Pattern
 * - Provides unified interface regardless of underlying implementation
 * - Allows gradual migration from std::thread to thread_system
 * - Zero runtime overhead when using compile-time selection
 *
 * ### Usage
 * @code
 * #include "adapters/thread_pool_adapter.h"
 *
 * // Use unified types
 * database::async::thread_pool_type pool;
 * database::async::thread_context_type context;
 * @endcode
 */

#ifdef USE_THREAD_SYSTEM
    // thread_system is available - use high-performance implementation
    #include <kcenon/thread/core/thread_pool.h>
    #include <kcenon/thread/core/typed_thread_pool.h>
    #include <kcenon/thread/core/job.h>
    #include <kcenon/thread/interfaces/thread_context.h>
    // Note: thread_system v3.0 removes monitoring_interface.h
    // Use common_system interfaces instead
    #include <kcenon/common/interfaces/logger_interface.h>
    #include <kcenon/common/interfaces/monitoring_interface.h>

    namespace database::async {
        /**
         * @brief Type alias for thread pool implementation
         * Uses kcenon::thread::thread_pool when thread_system is available
         */
        using thread_pool_type = kcenon::thread::thread_pool;

        /**
         * @brief Type alias for typed thread pool (priority-based)
         * Uses kcenon::thread::typed_thread_pool_t when thread_system is available
         */
        template<typename JobType = kcenon::thread::job_types>
        using typed_thread_pool_type = kcenon::thread::typed_thread_pool_t<JobType>;

        /**
         * @brief Type alias for job
         */
        using job_type = kcenon::thread::job;

        /**
         * @brief Type alias for typed job
         */
        template<typename JobType>
        using typed_job_type = kcenon::thread::typed_job_t<JobType>;

        /**
         * @brief Type alias for thread context
         */
        using thread_context_type = kcenon::thread::thread_context;

        /**
         * @brief Type alias for monitoring interface
         * @note Uses unified common_system interface instead of deprecated thread_system interface
         */
        using monitoring_interface_type = kcenon::common::interfaces::IMonitor;

        /**
         * @brief Type alias for logger interface
         * @note Uses unified common_system interface instead of deprecated thread_system interface
         */
        using logger_interface_type = kcenon::common::interfaces::ILogger;

        /**
         * @brief Type alias for Result<T> pattern
         * @note Uses unified common::Result<T> instead of deprecated thread_system result
         */
        template<typename T>
        using result_type = kcenon::common::Result<T>;

        /**
         * @brief Type alias for result_void
         * @note Uses unified common::VoidResult instead of deprecated thread_system result
         */
        using result_void_type = kcenon::common::VoidResult;

        /**
         * @brief Compile-time flag indicating thread_system is in use
         */
        constexpr bool using_thread_system = true;

    } // namespace database::async

#else
    // Fallback to standard library threading
    #include <thread>
    #include <mutex>
    #include <condition_variable>
    #include <atomic>
    #include <queue>
    #include <functional>
    #include <memory>
    #include <variant>
    #include <string>

    namespace database::async {
        /**
         * @brief Fallback thread context (empty implementation)
         * Provides a no-op context when thread_system is not available
         */
        class fallback_context {
        public:
            fallback_context() = default;
            ~fallback_context() = default;
            fallback_context(const fallback_context&) = default;
            fallback_context& operator=(const fallback_context&) = default;
            fallback_context(fallback_context&&) = default;
            fallback_context& operator=(fallback_context&&) = default;
        };

        // Forward declarations for fallback implementations
        class fallback_thread_pool;
        class fallback_job;

        /**
         * @brief Fallback thread pool using std::thread
         * Provides basic functionality when thread_system is not available
         */
        using thread_pool_type = fallback_thread_pool;

        /**
         * @brief Fallback typed thread pool (delegates to regular pool)
         */
        template<typename JobType>
        using typed_thread_pool_type = fallback_thread_pool;

        /**
         * @brief Fallback job type
         */
        using job_type = fallback_job;

        /**
         * @brief Fallback typed job
         */
        template<typename JobType>
        using typed_job_type = fallback_job;

        /**
         * @brief Fallback thread context (empty)
         */
        using thread_context_type = fallback_context;

        /**
         * @brief Fallback monitoring interface (no-op)
         */
        using monitoring_interface_type = void;

        /**
         * @brief Fallback logger interface (no-op)
         */
        using logger_interface_type = void;

        /**
         * @brief Fallback Result<T> implementation
         */
        template<typename T>
        class result_type {
        public:
            result_type(T&& value) : value_(std::forward<T>(value)), has_value_(true) {}
            result_type(const T& value) : value_(value), has_value_(true) {}
            result_type(const std::string& error) : error_(error), has_value_(false) {}

            bool is_ok() const { return has_value_; }
            bool has_error() const { return !has_value_; }

            const T& value() const { return value_; }
            T& value() { return value_; }

            const std::string& error() const { return error_; }

        private:
            T value_;
            std::string error_;
            bool has_value_;
        };

        /**
         * @brief Fallback result_void
         */
        class result_void_type {
        public:
            result_void_type() : has_error_(false) {}
            result_void_type(const std::string& error) : error_(error), has_error_(true) {}

            bool is_ok() const { return !has_error_; }
            bool has_error() const { return has_error_; }
            const std::string& error() const { return error_; }

        private:
            std::string error_;
            bool has_error_;
        };

        /**
         * @brief Compile-time flag indicating fallback mode
         */
        constexpr bool using_thread_system = false;

    } // namespace database::async

#endif // USE_THREAD_SYSTEM
