// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/adapters/backends/fallback_logger_backend.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{
	inline common::VoidResult make_error(const std::string& msg, int code = -1)
	{
		return common::VoidResult(common::error_info{ code, msg, "" });
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
	const char* log_level_to_string(database::integrated::db_log_level level)
	{
		using database::integrated::db_log_level;

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
}

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

fallback_logger_backend::fallback_logger_backend(const db_logger_config& config)
	: config_(config), initialized_(false)
{
}

fallback_logger_backend::~fallback_logger_backend()
{
	if (initialized_)
	{
		shutdown();
	}
}

common::VoidResult fallback_logger_backend::initialize()
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

common::VoidResult fallback_logger_backend::shutdown()
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

bool fallback_logger_backend::is_initialized() const
{
	return initialized_;
}

void fallback_logger_backend::log(db_log_level level, const std::string& message)
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

void fallback_logger_backend::flush()
{
	std::lock_guard<std::mutex> lock(mutex_);
	std::cout.flush();
	if (log_file_.is_open())
	{
		log_file_.flush();
	}
}

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace kcenon::database
