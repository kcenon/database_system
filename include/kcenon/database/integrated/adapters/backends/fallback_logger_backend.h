// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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

#include <kcenon/database/integrated/adapters/backends/logger_backend.h>

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
} // namespace kcenon::database
