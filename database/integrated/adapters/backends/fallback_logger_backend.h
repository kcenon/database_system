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
 * @file fallback_logger_backend.h
 * @brief Fallback logger backend using std::cout and std::ofstream
 *
 * Used when common_system logging is not available. Provides basic logging
 * functionality using standard C++ streams.
 *
 * Features:
 * - Console output via std::cout
 * - File output via std::ofstream
 * - Thread-safe operation with mutex
 * - Timestamp formatting
 */

#pragma once

#include "logger_backend.h"

#include <fstream>
#include <mutex>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class fallback_logger_backend
 * @brief Basic logger backend using standard C++ streams
 *
 * This backend provides simple logging when common_system logging is unavailable.
 * Uses std::cout for console output and std::ofstream for file output.
 */
class fallback_logger_backend : public logger_backend
{
public:
	/**
	 * @brief Construct fallback logger backend
	 * @param config Logger configuration
	 */
	explicit fallback_logger_backend(const db_logger_config& config);

	~fallback_logger_backend() override;

	// Non-copyable, non-movable
	fallback_logger_backend(const fallback_logger_backend&) = delete;
	fallback_logger_backend& operator=(const fallback_logger_backend&) = delete;
	fallback_logger_backend(fallback_logger_backend&&) = delete;
	fallback_logger_backend& operator=(fallback_logger_backend&&) = delete;

	common::VoidResult initialize() override;
	common::VoidResult shutdown() override;
	bool is_initialized() const override;
	void log(db_log_level level, const std::string& message) override;
	void flush() override;

private:
	const db_logger_config& config_;
	bool initialized_;
	std::mutex mutex_;
	std::ofstream log_file_;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
