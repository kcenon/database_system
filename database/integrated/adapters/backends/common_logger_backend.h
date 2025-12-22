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
 * @file common_logger_backend.h
 * @brief Logger backend using common_system's ILogger and GlobalLoggerRegistry
 *
 * Uses the common_system library for logging through the ILogger interface:
 * - Unified logging interface via GlobalLoggerRegistry
 * - LOG_* macros for convenient logging
 * - Runtime-bound logger implementations
 * - Thread-safe operation
 */

#pragma once

#include "logger_backend.h"

#include <memory>

#include <kcenon/database/config/feature_flags.h>

#if KCENON_HAS_COMMON_SYSTEM
#include <kcenon/common/interfaces/logger_interface.h>
#endif

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class common_logger_backend
 * @brief Logger backend using common_system's ILogger interface
 *
 * This backend uses the kcenon/common_system for logging through
 * the GlobalLoggerRegistry and ILogger interface.
 */
class common_logger_backend : public logger_backend
{
public:
	/**
	 * @brief Construct common logger backend
	 * @param config Logger configuration
	 */
	explicit common_logger_backend(const db_logger_config& config);

	~common_logger_backend() override;

	// Non-copyable, non-movable
	common_logger_backend(const common_logger_backend&) = delete;
	common_logger_backend& operator=(const common_logger_backend&) = delete;
	common_logger_backend(common_logger_backend&&) = delete;
	common_logger_backend& operator=(common_logger_backend&&) = delete;

	common::VoidResult initialize() override;
	common::VoidResult shutdown() override;
	bool is_initialized() const override;
	void log(db_log_level level, const std::string& message) override;
	void flush() override;

private:
#if KCENON_HAS_COMMON_SYSTEM
	/**
	 * @brief Convert db_log_level to common_system's log_level
	 */
	static kcenon::common::interfaces::log_level convert_log_level(db_log_level level);
#endif

	const db_logger_config& config_;
	bool initialized_;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
