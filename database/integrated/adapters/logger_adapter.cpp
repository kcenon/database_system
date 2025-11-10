// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

#include "logger_adapter.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Helper namespace for internal use
namespace
{
	inline common::VoidResult make_error(const std::string& msg, int code = -1)
	{
		return common::VoidResult(common::error_info{ code, msg, "" });
	}
}

// Conditional includes based on logger_system availability
#if defined(USE_LOGGER_SYSTEM)
	#include <kcenon/logger/core/logger.h>
	#include <kcenon/logger/writers/console_writer.h>
	#include <kcenon/logger/writers/file_writer.h>
#else
	#include <filesystem>
	#include <fstream>
	#include <iostream>
	#include <mutex>
#endif

namespace database
{
namespace integrated
{
namespace adapters
{

// ═══════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════

namespace
{

/**
 * @brief Sanitize SQL query for logging
 *
 * Removes sensitive information and truncates long queries.
 */
std::string sanitize_query(const std::string& query)
{
	std::string sanitized = query;

	// Remove password patterns
	const std::string patterns[]
		= { "PASSWORD '", "PASSWORD=", "password '", "password=", "PWD=", "pwd=" };

	for (const auto& pattern : patterns)
	{
		size_t pos = sanitized.find(pattern);
		if (pos != std::string::npos)
		{
			size_t end = sanitized.find_first_of("' \t\n;", pos + pattern.length());
			if (end != std::string::npos)
			{
				sanitized.replace(pos + pattern.length(), end - (pos + pattern.length()), "***");
			}
		}
	}

	// Truncate if too long
	const size_t max_length = 500;
	if (sanitized.length() > max_length)
	{
		sanitized = sanitized.substr(0, max_length) + "... [truncated]";
	}

	return sanitized;
}

/**
 * @brief Format timestamp for logging
 */
std::string format_timestamp()
{
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::stringstream ss;
	ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
	ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
	return ss.str();
}

/**
 * @brief Convert db_log_level to string
 */
const char* log_level_to_string(db_log_level level)
{
	switch (level)
	{
		case db_log_level::trace:
			return "TRACE";
		case db_log_level::debug:
			return "DEBUG";
		case db_log_level::info:
			return "INFO ";
		case db_log_level::warning:
			return "WARN ";
		case db_log_level::error:
			return "ERROR";
		case db_log_level::critical:
			return "CRIT ";
		case db_log_level::fatal:
			return "FATAL";
		default:
			return "UNKN ";
	}
}

#if defined(USE_LOGGER_SYSTEM)
/**
 * @brief Convert db_log_level to logger_system's log_level
 */
kcenon::logger::log_level convert_log_level(db_log_level level)
{
	switch (level)
	{
		case db_log_level::trace:
			return kcenon::logger::log_level::trace;
		case db_log_level::debug:
			return kcenon::logger::log_level::debug;
		case db_log_level::info:
			return kcenon::logger::log_level::info;
		case db_log_level::warning:
			return kcenon::logger::log_level::warning;
		case db_log_level::error:
			return kcenon::logger::log_level::error;
		case db_log_level::critical:
			return kcenon::logger::log_level::critical;
		case db_log_level::fatal:
			// logger_system doesn't have fatal, map to critical
			return kcenon::logger::log_level::critical;
		default:
			return kcenon::logger::log_level::info;
	}
}
#endif

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════
// PIMPL Implementation - WITH logger_system
// ═══════════════════════════════════════════════════════════════

#if defined(USE_LOGGER_SYSTEM)

class logger_adapter::impl
{
public:
	explicit impl(const db_logger_config& config)
		: config_(config), initialized_(false), logger_(nullptr)
	{
	}

	~impl()
	{
		if (initialized_)
		{
			shutdown();
		}
	}

	common::VoidResult initialize()
	{
		if (initialized_)
		{
			return common::ok();
		}

		try
		{
			// Create logger with async support and buffer size
			logger_ = std::make_unique<kcenon::logger::logger>(
				true,  // async mode for better performance
				8192   // buffer size
			);

			// Add console writer
			auto console = std::make_unique<kcenon::logger::console_writer>();
			auto add_console_result = logger_->add_writer(std::move(console));
			if (!add_console_result)
			{
				return make_error("Failed to add console writer");
			}

			// Add file writer if enabled
			if (config_.enable_file_logging)
			{
				std::string log_file = config_.log_directory + "/database.log";
				auto file_writer = std::make_unique<kcenon::logger::file_writer>(log_file);
				auto add_file_result = logger_->add_writer(std::move(file_writer));
				if (!add_file_result)
				{
					return make_error("Failed to add file writer");
				}
			}

			// Set minimum log level
			logger_->set_min_level(convert_log_level(config_.min_log_level));

			// Start the logger
			auto start_result = logger_->start();
			if (!start_result)
			{
				return make_error("Failed to start logger");
			}

			initialized_ = true;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Logger initialization failed: ") + e.what());
		}
	}

	common::VoidResult shutdown()
	{
		if (!initialized_)
		{
			return common::ok();
		}

		try
		{
			if (logger_)
			{
				flush();
				auto stop_result = logger_->stop();
				if (!stop_result)
				{
					return make_error("Failed to stop logger");
				}
				logger_.reset();
			}
			initialized_ = false;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Logger shutdown failed: ") + e.what());
		}
	}

	bool is_initialized() const
	{
		return initialized_;
	}

	void log(db_log_level level, const std::string& message)
	{
		if (!initialized_ || !logger_)
		{
			return;
		}

		// Check if this level should be logged
		if (level < config_.min_log_level)
		{
			return;
		}

		logger_->log(convert_log_level(level), message);
	}

	void flush()
	{
		if (logger_)
		{
			logger_->flush();
		}
	}

private:
	const db_logger_config& config_;
	bool initialized_;
	std::unique_ptr<kcenon::logger::logger> logger_;
};

#else

// ═══════════════════════════════════════════════════════════════
// PIMPL Implementation - Fallback (std::cout + std::ofstream)
// ═══════════════════════════════════════════════════════════════

class logger_adapter::impl
{
public:
	explicit impl(const db_logger_config& config)
		: config_(config), initialized_(false)
	{
	}

	~impl()
	{
		if (initialized_)
		{
			shutdown();
		}
	}

	common::VoidResult initialize()
	{
		if (initialized_)
		{
			return common::ok();
		}

		try
		{
			// Open log file if file logging enabled
			if (config_.enable_file_logging)
			{
				// Create directory if it doesn't exist
				std::filesystem::path log_dir(config_.log_directory);
				if (!std::filesystem::exists(log_dir))
				{
					std::filesystem::create_directories(log_dir);
				}

				std::string log_path = config_.log_directory + "/database.log";
				log_file_.open(log_path, std::ios::app);
				if (!log_file_.is_open())
				{
					return make_error("Failed to open log file: " + log_path);
				}
			}

			initialized_ = true;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Logger initialization failed: ") + e.what());
		}
	}

	common::VoidResult shutdown()
	{
		if (!initialized_)
		{
			return common::ok();
		}

		try
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (log_file_.is_open())
			{
				log_file_.flush();
				log_file_.close();
			}
			initialized_ = false;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Logger shutdown failed: ") + e.what());
		}
	}

	bool is_initialized() const
	{
		return initialized_;
	}

	void log(db_log_level level, const std::string& message)
	{
		if (!initialized_)
		{
			return;
		}

		// Check if this level should be logged
		if (level < config_.min_log_level)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);

		// Format: [2025-01-03 14:30:45.123] [INFO ] message
		std::string log_line
			= "[" + format_timestamp() + "] [" + log_level_to_string(level) + "] " + message;

		// Write to console
		std::cout << log_line << std::endl;

		// Write to file if enabled
		if (config_.enable_file_logging && log_file_.is_open())
		{
			log_file_ << log_line << std::endl;
		}
	}

	void flush()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::cout.flush();
		if (log_file_.is_open())
		{
			log_file_.flush();
		}
	}

private:
	const db_logger_config& config_;
	bool initialized_;
	std::mutex mutex_;
	std::ofstream log_file_;
};

#endif

// ═══════════════════════════════════════════════════════════════
// Public Interface Implementation
// ═══════════════════════════════════════════════════════════════

logger_adapter::logger_adapter(const db_logger_config& config)
	: pimpl_(std::make_unique<impl>(config))
{
}

logger_adapter::~logger_adapter()
{
	if (pimpl_ && pimpl_->is_initialized())
	{
		pimpl_->shutdown();
	}
}

logger_adapter::logger_adapter(logger_adapter&&) noexcept = default;
logger_adapter& logger_adapter::operator=(logger_adapter&&) noexcept = default;

common::VoidResult logger_adapter::initialize()
{
	return pimpl_->initialize();
}

common::VoidResult logger_adapter::shutdown()
{
	return pimpl_->shutdown();
}

bool logger_adapter::is_initialized() const
{
	return pimpl_ && pimpl_->is_initialized();
}

// ═══════════════════════════════════════════════════════════════
// Database-Specific Logging
// ═══════════════════════════════════════════════════════════════

void logger_adapter::log_query(
	db_log_level level, const std::string& query, std::chrono::microseconds duration)
{
	if (!pimpl_)
	{
		return;
	}

	// Sanitize query
	std::string safe_query = sanitize_query(query);

	// Format message
	std::stringstream ss;
	ss << "Query executed in " << duration.count() << "μs: " << safe_query;

	pimpl_->log(level, ss.str());

	// Check for slow query
	auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	if (duration_ms > std::chrono::milliseconds(1000)) // Basic threshold
	{
		// Check config threshold
		db_logger_config dummy_config;
		// Note: We can't access config_ here, so we use a heuristic
		// In practice, connection_pool_v2 should call log_slow_query explicitly
	}
}

void logger_adapter::log_slow_query(const std::string& query, std::chrono::microseconds duration,
	std::chrono::milliseconds threshold)
{
	if (!pimpl_)
	{
		return;
	}

	std::string safe_query = sanitize_query(query);

	std::stringstream ss;
	ss << "SLOW QUERY detected (threshold: " << threshold.count() << "ms, actual: "
	   << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
	   << "ms): " << safe_query;

	pimpl_->log(db_log_level::warning, ss.str());
}

void logger_adapter::log_connection_event(const std::string& event, const std::string& details)
{
	if (!pimpl_)
	{
		return;
	}

	std::stringstream ss;
	ss << "Connection event [" << event << "]: " << details;

	pimpl_->log(db_log_level::debug, ss.str());
}

void logger_adapter::log_transaction(
	const std::string& operation, bool success, const std::string& details)
{
	if (!pimpl_)
	{
		return;
	}

	std::stringstream ss;
	ss << "Transaction " << operation << " " << (success ? "SUCCESS" : "FAILED");
	if (!details.empty())
	{
		ss << ": " << details;
	}

	pimpl_->log(success ? db_log_level::info : db_log_level::error, ss.str());
}

void logger_adapter::log_pool_event(const std::string& event, std::size_t active, std::size_t idle)
{
	if (!pimpl_)
	{
		return;
	}

	std::stringstream ss;
	ss << "Pool event [" << event << "]: active=" << active << ", idle=" << idle
	   << ", total=" << (active + idle);

	pimpl_->log(db_log_level::info, ss.str());
}

void logger_adapter::log_error(
	const std::string& operation, const std::string& error_msg, const std::string& sql_state)
{
	if (!pimpl_)
	{
		return;
	}

	std::stringstream ss;
	ss << "ERROR in " << operation << ": " << error_msg;
	if (!sql_state.empty())
	{
		ss << " (SQL state: " << sql_state << ")";
	}

	pimpl_->log(db_log_level::error, ss.str());
}

void logger_adapter::log(db_log_level level, const std::string& message)
{
	if (pimpl_)
	{
		pimpl_->log(level, message);
	}
}

void logger_adapter::flush()
{
	if (pimpl_)
	{
		pimpl_->flush();
	}
}

} // namespace adapters
} // namespace integrated
} // namespace database
