// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/adapters/backends/fallback_thread_backend.h>

#include <kcenon/database/core/result.h>

#include <algorithm>

namespace
{
	// In-band database_system code (see core/result.h) so the shared
	// common::error_info::code resolves to "DatabaseSystem", not the common
	// (-1..-99) band.
	inline common::VoidResult make_error(const std::string& msg)
	{
		return common::VoidResult(common::error_info{
			static_cast<int>(kcenon::database::error_code::unknown_error), msg, "" });
	}
}

namespace kcenon::database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

fallback_thread_backend::fallback_thread_backend(const db_thread_config& config)
	: config_(config)
	, initialized_(false)
	, shutdown_requested_(false)
	, active_tasks_(0)
{
}

fallback_thread_backend::~fallback_thread_backend()
{
	if (initialized_)
	{
		shutdown();
	}
}

common::VoidResult fallback_thread_backend::initialize()
{
	if (initialized_)
	{
		return common::ok();
	}

	try
	{
		shutdown_requested_ = false;
		active_tasks_ = 0;

		// Determine worker count
		std::size_t worker_count = config_.thread_count;
		if (worker_count == 0)
		{
			worker_count = std::max(1u, std::thread::hardware_concurrency());
		}

		// Create worker threads
		workers_.reserve(worker_count);
		for (std::size_t i = 0; i < worker_count; ++i)
		{
			workers_.emplace_back(&fallback_thread_backend::worker_thread, this);
		}

		initialized_ = true;
		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error(std::string("Thread pool initialization failed: ") + e.what());
	}
}

common::VoidResult fallback_thread_backend::shutdown()
{
	if (!initialized_)
	{
		return common::ok();
	}

	try
	{
		// Signal shutdown
		shutdown_requested_ = true;

		// Wake up all workers
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			queue_cv_.notify_all();
		}

		// Wait for all workers to finish
		for (auto& worker : workers_)
		{
			if (worker.joinable())
			{
				worker.join();
			}
		}

		workers_.clear();

		// Clear remaining tasks
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			std::queue<std::function<void()>> empty;
			task_queue_.swap(empty);
		}

		initialized_ = false;
		return common::ok();
	}
	catch (const std::exception& e)
	{
		return make_error(std::string("Thread pool shutdown failed: ") + e.what());
	}
}

bool fallback_thread_backend::is_initialized() const
{
	return initialized_;
}

common::VoidResult fallback_thread_backend::execute(std::function<void()> task)
{
	if (!initialized_)
	{
		return make_error("Thread pool not initialized");
	}

	if (!task)
	{
		return make_error("Invalid task");
	}

	if (shutdown_requested_)
	{
		return make_error("Thread pool is shutting down");
	}

	{
		std::lock_guard<std::mutex> lock(queue_mutex_);

		// Check queue size limit
		if (config_.max_queue_size > 0 && task_queue_.size() >= config_.max_queue_size)
		{
			return make_error("Task queue full");
		}

		task_queue_.push(std::move(task));
	}

	queue_cv_.notify_one();
	return common::ok();
}

void fallback_thread_backend::wait_for_completion()
{
	if (!initialized_)
	{
		return;
	}

	std::unique_lock<std::mutex> lock(completion_mutex_);
	completion_cv_.wait(lock, [this]() {
		std::lock_guard<std::mutex> queue_lock(queue_mutex_);
		return task_queue_.empty() && active_tasks_ == 0;
	});
}

bool fallback_thread_backend::wait_for_completion_timeout(std::chrono::milliseconds timeout)
{
	if (!initialized_)
	{
		return true;
	}

	std::unique_lock<std::mutex> lock(completion_mutex_);
	return completion_cv_.wait_for(lock, timeout, [this]() {
		std::lock_guard<std::mutex> queue_lock(queue_mutex_);
		return task_queue_.empty() && active_tasks_ == 0;
	});
}

std::size_t fallback_thread_backend::worker_count() const
{
	return workers_.size();
}

std::size_t fallback_thread_backend::queue_size() const
{
	std::lock_guard<std::mutex> lock(queue_mutex_);
	return task_queue_.size();
}

bool fallback_thread_backend::is_idle() const
{
	std::lock_guard<std::mutex> lock(queue_mutex_);
	return task_queue_.empty() && active_tasks_ == 0;
}

void fallback_thread_backend::worker_thread()
{
	while (true)
	{
		std::function<void()> task;

		{
			std::unique_lock<std::mutex> lock(queue_mutex_);

			// Wait for task or shutdown
			queue_cv_.wait(lock, [this]() {
				return shutdown_requested_ || !task_queue_.empty();
			});

			// Check shutdown
			if (shutdown_requested_ && task_queue_.empty())
			{
				break;
			}

			// Get next task
			if (!task_queue_.empty())
			{
				task = std::move(task_queue_.front());
				task_queue_.pop();
			}
		}

		// Execute task
		if (task)
		{
			++active_tasks_;

			try
			{
				task();
			}
			catch (...)
			{
				// Swallow exceptions to prevent worker thread from terminating
			}

			--active_tasks_;

			// Notify completion waiters
			{
				std::lock_guard<std::mutex> lock(completion_mutex_);
				completion_cv_.notify_all();
			}
		}
	}
}

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace kcenon::database
