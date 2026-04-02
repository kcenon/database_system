// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
