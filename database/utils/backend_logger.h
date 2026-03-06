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
 * @file backend_logger.h
 * @brief Centralized logging utility for database backends
 *
 * This file provides a unified logging interface for all database backends,
 * eliminating duplicate logging macro definitions across backend implementations.
 *
 * Issue #327: Extract centralized backend logger to eliminate logging macro duplication
 */

#pragma once

#include <kcenon/database/config/feature_flags.h>

#if KCENON_HAS_COMMON_SYSTEM
#include <kcenon/common/logging/log_functions.h>
#else
#include <iostream>
#endif

#include <string>
#include <string_view>

namespace database
{
namespace utils
{

/**
 * @class backend_logger
 * @brief Centralized logging utility for database backends
 *
 * This class provides a unified logging interface that replaces the
 * duplicate *_LOG_ERROR, *_LOG_WARNING, and *_LOG_INFO macros
 * previously defined in each backend implementation.
 *
 * Usage:
 * @code
 *   namespace {
 *       const database::utils::backend_logger logger_("PostgreSQL");
 *   }
 *
 *   void some_function() {
 *       logger_.error("initialize", "Connection failed");
 *       logger_.warning("Mock mode enabled");
 *       logger_.info("Connection established");
 *   }
 * @endcode
 */
class backend_logger
{
public:
	/**
	 * @brief Construct a logger for a specific backend
	 * @param backend_name Name of the backend (e.g., "PostgreSQL", "SQLite")
	 */
	explicit backend_logger(std::string_view backend_name)
		: backend_name_(backend_name)
	{
	}

	/**
	 * @brief Log an error message with context
	 * @param context The operation context (e.g., "initialize", "execute_query")
	 * @param message The error message
	 */
	void error(std::string_view context, std::string_view message) const
	{
		std::string formatted
			= "[" + backend_name_ + ":" + std::string(context) + "] " + std::string(message);
#if KCENON_HAS_COMMON_SYSTEM
		kcenon::common::logging::log_error(formatted);
#else
		std::cerr << formatted << std::endl;
#endif
	}

	/**
	 * @brief Log a warning message
	 * @param message The warning message
	 */
	void warning(std::string_view message) const
	{
		std::string formatted = "[" + backend_name_ + "] " + std::string(message);
#if KCENON_HAS_COMMON_SYSTEM
		kcenon::common::logging::log_warning(formatted);
#else
		std::cerr << formatted << std::endl;
#endif
	}

	/**
	 * @brief Log an info message
	 * @param message The info message
	 */
	void info(std::string_view message) const
	{
		std::string formatted = "[" + backend_name_ + "] " + std::string(message);
#if KCENON_HAS_COMMON_SYSTEM
		kcenon::common::logging::log_info(formatted);
#else
		std::cout << formatted << std::endl;
#endif
	}

private:
	std::string backend_name_;
};

} // namespace utils
} // namespace database
