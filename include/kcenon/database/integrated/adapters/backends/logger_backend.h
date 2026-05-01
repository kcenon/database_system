// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file logger_backend.h
 * @brief Abstract interface for logger backends
 *
 * Defines the interface that all logger backends must implement.
 * This enables runtime selection of logging implementation without
 * conditional compilation.
 */

#pragma once

#include <kcenon/database/integrated/core/common_result.h>
#include <kcenon/database/integrated/core/configuration.h>

#include <string>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class logger_backend
 * @brief Abstract base class for logger backends
 *
 * All logger backends (system, fallback, null) must implement this interface.
 * This enables runtime polymorphism and eliminates conditional compilation.
 */
class logger_backend
{
public:
	virtual ~logger_backend() = default;

	/**
	 * @brief Initialize the logger backend
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult initialize() = 0;

	/**
	 * @brief Shutdown the logger backend gracefully
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult shutdown() = 0;

	/**
	 * @brief Check if backend is initialized
	 * @return true if initialized and ready to log
	 */
	virtual bool is_initialized() const = 0;

	/**
	 * @brief Log a message
	 * @param level Log level
	 * @param message Message to log
	 */
	virtual void log(db_log_level level, const std::string& message) = 0;

	/**
	 * @brief Flush pending log messages
	 */
	virtual void flush() = 0;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
