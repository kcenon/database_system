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
 * @file system_logger_backend.h
 * @brief Logger backend using kcenon/logger_system
 *
 * Uses the logger_system library for advanced logging features:
 * - Asynchronous logging
 * - Multiple writers (console, file, etc.)
 * - Structured log levels
 * - High performance
 */

#pragma once

#include "logger_backend.h"

#include <memory>

// Forward declarations to avoid header dependency when logger_system is unavailable
namespace kcenon
{
namespace logger
{
	class logger;
}
}

// Import log_level from logger_system
// Note: We can't forward-declare this because logger.h has a using declaration
#include <kcenon/logger/interfaces/logger_types.h>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class system_logger_backend
 * @brief Logger backend using logger_system library
 *
 * This backend uses the kcenon/logger_system for production-grade logging.
 * Requires logger_system to be available at compile time.
 */
class system_logger_backend : public logger_backend
{
public:
	/**
	 * @brief Construct system logger backend
	 * @param config Logger configuration
	 */
	explicit system_logger_backend(const db_logger_config& config);

	~system_logger_backend() override;

	// Non-copyable, non-movable (holds unique logger instance)
	system_logger_backend(const system_logger_backend&) = delete;
	system_logger_backend& operator=(const system_logger_backend&) = delete;
	system_logger_backend(system_logger_backend&&) = delete;
	system_logger_backend& operator=(system_logger_backend&&) = delete;

	common::VoidResult initialize() override;
	common::VoidResult shutdown() override;
	bool is_initialized() const override;
	void log(db_log_level level, const std::string& message) override;
	void flush() override;

private:
	/**
	 * @brief Convert db_log_level to logger_system's log_level
	 */
	static logger_system::log_level convert_log_level(db_log_level level);

	const db_logger_config& config_;
	bool initialized_;
	std::unique_ptr<kcenon::logger::logger> logger_;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
