// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

#include "database_coordinator.h"

#include "../adapters/logger_adapter.h"
#include "../adapters/monitoring_adapter.h"
#include "../adapters/thread_adapter.h"

#include <chrono>
#include <stdexcept>

namespace database
{
namespace integrated
{

// Helper to create error result
namespace
{
	inline common::VoidResult make_error(const std::string& msg, int code = -1)
	{
#if defined(USE_COMMON_SYSTEM)
		return common::VoidResult(common::error_info{ code, msg });
#else
		return common::VoidResult(common::Error{ msg, code });
#endif
	}
}

// ═══════════════════════════════════════════════════════════════
// PIMPL Implementation
// ═══════════════════════════════════════════════════════════════

class database_coordinator::impl
{
public:
	explicit impl(const unified_db_config& config)
		: config_(config)
		, initialized_(false)
		, logger_(nullptr)
		, monitor_(nullptr)
		, thread_pool_(nullptr)
		, init_time_()
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
			return make_error("Coordinator already initialized");
		}

		init_time_ = std::chrono::system_clock::now();

		try
		{
			// ═══════════════════════════════════════════════════════════════
			// Phase 1: Initialize Logger (for observability of other inits)
			// ═══════════════════════════════════════════════════════════════
			logger_ = std::make_unique<adapters::logger_adapter>(config_.logger);

			auto logger_result = logger_->initialize();
			if (!logger_result.is_ok())
			{
				logger_.reset();
				return make_error(
					"Logger initialization failed: "
#if defined(USE_COMMON_SYSTEM)
						+ logger_result.error().message
#else
						+ logger_result.error().message
#endif
				);
			}

			logger_->log(db_log_level::info, "Database coordinator: Logger initialized");

			// ═══════════════════════════════════════════════════════════════
			// Phase 2: Initialize Monitoring (for metrics collection)
			// ═══════════════════════════════════════════════════════════════
			monitor_ = std::make_unique<adapters::monitoring_adapter>(config_.monitoring);

			auto monitor_result = monitor_->initialize();
			if (!monitor_result.is_ok())
			{
				logger_->log(db_log_level::error, "Monitoring initialization failed");

				// Rollback: shutdown logger
				logger_->shutdown();
				logger_.reset();
				monitor_.reset();

				return make_error(
					"Monitoring initialization failed: "
#if defined(USE_COMMON_SYSTEM)
						+ monitor_result.error().message
#else
						+ monitor_result.error().message
#endif
				);
			}

			logger_->log(db_log_level::info, "Database coordinator: Monitoring initialized");

			// ═══════════════════════════════════════════════════════════════
			// Phase 3: Initialize Thread Pool (for async operations)
			// ═══════════════════════════════════════════════════════════════
			thread_pool_ = std::make_unique<adapters::thread_adapter>(config_.thread);

			auto thread_result = thread_pool_->initialize();
			if (!thread_result.is_ok())
			{
				logger_->log(db_log_level::error, "Thread pool initialization failed");

				// Rollback: shutdown monitor and logger
				monitor_->shutdown();
				monitor_.reset();
				logger_->shutdown();
				logger_.reset();
				thread_pool_.reset();

				return make_error(
					"Thread pool initialization failed: "
#if defined(USE_COMMON_SYSTEM)
						+ thread_result.error().message
#else
						+ thread_result.error().message
#endif
				);
			}

			logger_->log(db_log_level::info, "Database coordinator: Thread pool initialized");

			// ═══════════════════════════════════════════════════════════════
			// Initialization Complete
			// ═══════════════════════════════════════════════════════════════
			initialized_ = true;

			logger_->log(db_log_level::info,
						 "Database coordinator: All adapters initialized successfully");

			return common::ok();
		}
		catch (const std::exception& e)
		{
			// Clean up any partially initialized adapters
			if (thread_pool_)
			{
				thread_pool_->shutdown();
				thread_pool_.reset();
			}
			if (monitor_)
			{
				monitor_->shutdown();
				monitor_.reset();
			}
			if (logger_)
			{
				logger_->log(db_log_level::error,
							 std::string("Exception during initialization: ") + e.what());
				logger_->shutdown();
				logger_.reset();
			}

			return make_error(std::string("Exception during initialization: ") + e.what());
		}
	}

	common::VoidResult shutdown()
	{
		if (!initialized_)
		{
			return common::ok(); // Already shut down
		}

		try
		{
			// Shutdown in REVERSE order of initialization
			// This ensures dependencies are torn down safely

			// ═══════════════════════════════════════════════════════════════
			// Phase 1: Shutdown Thread Pool
			// ═══════════════════════════════════════════════════════════════
			if (thread_pool_)
			{
				if (logger_)
				{
					logger_->log(db_log_level::info,
								 "Database coordinator: Shutting down thread pool");
				}

				auto thread_result = thread_pool_->shutdown();
				if (!thread_result.is_ok())
				{
					if (logger_)
					{
						logger_->log(db_log_level::warning,
									 "Thread pool shutdown warning: "
#if defined(USE_COMMON_SYSTEM)
										 + thread_result.error().message
#else
										 + thread_result.error().message
#endif
						);
					}
				}

				thread_pool_.reset();
			}

			// ═══════════════════════════════════════════════════════════════
			// Phase 2: Shutdown Monitoring
			// ═══════════════════════════════════════════════════════════════
			if (monitor_)
			{
				if (logger_)
				{
					logger_->log(db_log_level::info,
								 "Database coordinator: Shutting down monitoring");
				}

				auto monitor_result = monitor_->shutdown();
				if (!monitor_result.is_ok())
				{
					if (logger_)
					{
						logger_->log(db_log_level::warning,
									 "Monitoring shutdown warning: "
#if defined(USE_COMMON_SYSTEM)
										 + monitor_result.error().message
#else
										 + monitor_result.error().message
#endif
						);
					}
				}

				monitor_.reset();
			}

			// ═══════════════════════════════════════════════════════════════
			// Phase 3: Shutdown Logger (LAST - keep logging until the end)
			// ═══════════════════════════════════════════════════════════════
			if (logger_)
			{
				logger_->log(db_log_level::info,
							 "Database coordinator: Shutdown complete");
				logger_->flush(); // Ensure all logs are written

				auto logger_result = logger_->shutdown();
				// Don't log logger shutdown errors (logger is shutting down!)

				logger_.reset();
			}

			initialized_ = false;

			return common::ok();
		}
		catch (const std::exception& e)
		{
			// Best effort cleanup
			thread_pool_.reset();
			monitor_.reset();
			logger_.reset();

			initialized_ = false;

			return make_error(std::string("Exception during shutdown: ") + e.what());
		}
	}

	bool is_initialized() const
	{
		return initialized_;
	}

	adapters::logger_adapter* get_logger()
	{
		return logger_.get();
	}

	adapters::monitoring_adapter* get_monitor()
	{
		return monitor_.get();
	}

	adapters::thread_adapter* get_thread_pool()
	{
		return thread_pool_.get();
	}

	common::Result<bool> check_health()
	{
		if (!initialized_)
		{
			return common::Result<bool>(
#if defined(USE_COMMON_SYSTEM)
				common::error_info{ -1, "Coordinator not initialized" }
#else
				common::Error{ "Coordinator not initialized", -1 }
#endif
			);
		}

		bool overall_healthy = true;

		// Check logger
		if (!logger_)
		{
			overall_healthy = false;
		}

		// Check monitoring
		if (!monitor_)
		{
			overall_healthy = false;
		}
		else
		{
			auto monitor_health = monitor_->perform_health_check();
			if (!monitor_health.is_ok() || !monitor_health.value())
			{
				overall_healthy = false;
				if (logger_)
				{
					logger_->log(db_log_level::warning, "Monitoring health check failed");
				}
			}
		}

		// Check thread pool
		if (!thread_pool_)
		{
			overall_healthy = false;
		}
		else
		{
			// Thread pool health check: verify statistics are accessible
			auto stats_result = thread_pool_->get_statistics();
			if (!stats_result.is_ok())
			{
				overall_healthy = false;
				if (logger_)
				{
					logger_->log(db_log_level::warning, "Thread pool health check failed");
				}
			}
		}

		return common::Result<bool>(overall_healthy);
	}

	common::Result<database_coordinator::coordinator_stats> get_stats() const
	{
		coordinator_stats stats;
		stats.is_initialized = initialized_;
		stats.logger_healthy = (logger_ != nullptr);
		stats.monitoring_healthy = (monitor_ != nullptr);
		stats.thread_pool_healthy = (thread_pool_ != nullptr);
		stats.init_time = init_time_;

		if (initialized_)
		{
			auto now = std::chrono::system_clock::now();
			stats.uptime
				= std::chrono::duration_cast<std::chrono::milliseconds>(now - init_time_);
		}
		else
		{
			stats.uptime = std::chrono::milliseconds(0);
		}

		return common::Result<coordinator_stats>(stats);
	}

private:
	unified_db_config config_;
	bool initialized_;

	// Adapters (in initialization order)
	std::unique_ptr<adapters::logger_adapter> logger_;
	std::unique_ptr<adapters::monitoring_adapter> monitor_;
	std::unique_ptr<adapters::thread_adapter> thread_pool_;

	// Statistics
	std::chrono::system_clock::time_point init_time_;
};

// ═══════════════════════════════════════════════════════════════
// Public Interface
// ═══════════════════════════════════════════════════════════════

database_coordinator::database_coordinator(const unified_db_config& config)
	: pimpl_(std::make_unique<impl>(config))
{
}

database_coordinator::~database_coordinator() = default;

database_coordinator::database_coordinator(database_coordinator&&) noexcept = default;
database_coordinator& database_coordinator::operator=(database_coordinator&&) noexcept = default;

common::VoidResult database_coordinator::initialize()
{
	return pimpl_->initialize();
}

common::VoidResult database_coordinator::shutdown()
{
	return pimpl_->shutdown();
}

bool database_coordinator::is_initialized() const
{
	return pimpl_->is_initialized();
}

adapters::logger_adapter* database_coordinator::get_logger()
{
	return pimpl_->get_logger();
}

adapters::monitoring_adapter* database_coordinator::get_monitor()
{
	return pimpl_->get_monitor();
}

adapters::thread_adapter* database_coordinator::get_thread_pool()
{
	return pimpl_->get_thread_pool();
}

common::Result<bool> database_coordinator::check_health()
{
	return pimpl_->check_health();
}

common::Result<database_coordinator::coordinator_stats> database_coordinator::get_stats() const
{
	return pimpl_->get_stats();
}

} // namespace integrated
} // namespace database
