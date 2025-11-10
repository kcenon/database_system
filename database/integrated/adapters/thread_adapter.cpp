// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

#include "thread_adapter.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

// Conditional includes based on thread_system availability
#if defined(USE_THREAD_SYSTEM)
	#include <kcenon/thread/core/thread_pool.h>
	#include <kcenon/thread/core/thread_worker.h>
	#include <kcenon/thread/interfaces/thread_context.h>
#endif

namespace database
{
namespace integrated
{
namespace adapters
{

// ═══════════════════════════════════════════════════════════════
// Cancellation Token Implementation
// ═══════════════════════════════════════════════════════════════

class cancellation_token::impl
{
public:
	impl() : cancelled_(false)
	{
	}

	void cancel()
	{
		cancelled_.store(true, std::memory_order_release);
	}

	bool is_cancelled() const
	{
		return cancelled_.load(std::memory_order_acquire);
	}

private:
	std::atomic<bool> cancelled_;
};

cancellation_token::cancellation_token() : pimpl_(std::make_shared<impl>())
{
}

cancellation_token::~cancellation_token() = default;

void cancellation_token::cancel()
{
	if (pimpl_)
	{
		pimpl_->cancel();
	}
}

bool cancellation_token::is_cancelled() const
{
	return pimpl_ && pimpl_->is_cancelled();
}

// ═══════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════

namespace
{

inline common::VoidResult make_error(const std::string& msg, int code = -1)
{
	return common::VoidResult(common::error_info{ code, msg, "" });
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════
// Thread Adapter PIMPL Implementation
// ═══════════════════════════════════════════════════════════════

#if defined(USE_THREAD_SYSTEM)

// ───────────────────────────────────────────────────────────────
// WITH thread_system
// ───────────────────────────────────────────────────────────────

class thread_adapter::impl
{
public:
	explicit impl(const db_thread_config& config) : config_(config), initialized_(false)
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
			std::size_t thread_count = config_.thread_count;
			if (thread_count == 0)
			{
				thread_count = std::thread::hardware_concurrency();
				if (thread_count == 0)
				{
					thread_count = 4; // Fallback
				}
			}

			// Create thread_pool
			std::string pool_name
				= config_.pool_name.empty() ? "db_thread_pool" : config_.pool_name;
			thread_pool_ = std::make_shared<kcenon::thread::thread_pool>(pool_name);

			// Add worker threads
			for (std::size_t i = 0; i < thread_count; ++i)
			{
				auto worker = std::make_unique<kcenon::thread::thread_worker>(
					true, // use time tag
					kcenon::thread::thread_context());

				worker->set_job_queue(thread_pool_->get_job_queue());

				auto enqueue_result = thread_pool_->enqueue(std::move(worker));
				if (enqueue_result.has_error())
				{
					return make_error("Failed to add worker thread");
				}
			}

			// Start the pool
			auto start_result = thread_pool_->start();
			if (start_result.has_error())
			{
				return make_error("Failed to start thread pool");
			}

			initialized_ = true;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Thread adapter initialization failed: ") + e.what());
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
			bool success = thread_pool_->shutdown_pool(false); // Graceful shutdown
			if (!success)
			{
				return make_error("Failed to shutdown thread pool");
			}

			thread_pool_.reset();
			initialized_ = false;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Thread shutdown failed: ") + e.what());
		}
	}

	bool is_initialized() const
	{
		return initialized_;
	}

	common::VoidResult execute(std::function<void()> task)
	{
		if (!initialized_)
		{
			return make_error("Thread adapter not initialized");
		}

		bool success = thread_pool_->submit_task(std::move(task));
		if (!success)
		{
			return make_error("Failed to enqueue task");
		}

		return common::ok();
	}

	void wait_for_completion()
	{
		// Poll until queue is empty
		while (thread_pool_ && thread_pool_->get_pending_task_count() > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	bool wait_for_completion_timeout(std::chrono::milliseconds timeout)
	{
		auto start = std::chrono::steady_clock::now();
		while (thread_pool_ && thread_pool_->get_pending_task_count() > 0)
		{
			auto elapsed = std::chrono::steady_clock::now() - start;
			if (elapsed >= timeout)
			{
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return true;
	}

	std::size_t worker_count() const
	{
		return thread_pool_ ? thread_pool_->get_thread_count() : 0;
	}

	std::size_t queue_size() const
	{
		return thread_pool_ ? thread_pool_->get_pending_task_count() : 0;
	}

	bool is_idle() const
	{
		return queue_size() == 0;
	}

	std::shared_ptr<cancellation_token> create_cancellation_token()
	{
		return std::make_shared<cancellation_token>();
	}

private:
	const db_thread_config& config_;
	bool initialized_;
	std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;
};

#else

// ───────────────────────────────────────────────────────────────
// Fallback (std::thread + std::queue)
// ───────────────────────────────────────────────────────────────

class thread_adapter::impl
{
public:
	explicit impl(const db_thread_config& config)
		: config_(config), initialized_(false), shutdown_(false), active_tasks_(0)
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
			std::size_t thread_count = config_.thread_count;
			if (thread_count == 0)
			{
				thread_count = std::thread::hardware_concurrency();
				if (thread_count == 0)
				{
					thread_count = 4;
				}
			}

			// Create worker threads
			workers_.reserve(thread_count);
			for (std::size_t i = 0; i < thread_count; ++i)
			{
				workers_.emplace_back([this] { worker_thread(); });
			}

			initialized_ = true;
			return common::ok();
		}
		catch (const std::exception& e)
		{
			return make_error(std::string("Thread adapter initialization failed: ") + e.what());
		}
	}

	common::VoidResult shutdown()
	{
		if (!initialized_)
		{
			return common::ok();
		}

		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			shutdown_ = true;
		}
		condition_.notify_all();

		// Wait for all workers to finish
		for (auto& worker : workers_)
		{
			if (worker.joinable())
			{
				worker.join();
			}
		}
		workers_.clear();

		initialized_ = false;
		return common::ok();
	}

	bool is_initialized() const
	{
		return initialized_;
	}

	common::VoidResult execute(std::function<void()> task)
	{
		if (!initialized_)
		{
			return make_error("Thread adapter not initialized");
		}

		{
			std::unique_lock<std::mutex> lock(queue_mutex_);

			if (shutdown_)
			{
				return make_error("Thread adapter is shutting down");
			}

			if (config_.max_queue_size > 0 && task_queue_.size() >= config_.max_queue_size)
			{
				return make_error("Task queue is full");
			}

			task_queue_.push(std::move(task));
		}

		condition_.notify_one();
		return common::ok();
	}

	void wait_for_completion()
	{
		std::unique_lock<std::mutex> lock(queue_mutex_);
		completion_cv_.wait(lock, [this] { return task_queue_.empty() && active_tasks_ == 0; });
	}

	bool wait_for_completion_timeout(std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lock(queue_mutex_);
		return completion_cv_.wait_for(
			lock, timeout, [this] { return task_queue_.empty() && active_tasks_ == 0; });
	}

	std::size_t worker_count() const
	{
		return workers_.size();
	}

	std::size_t queue_size() const
	{
		std::unique_lock<std::mutex> lock(queue_mutex_);
		return task_queue_.size();
	}

	bool is_idle() const
	{
		std::unique_lock<std::mutex> lock(queue_mutex_);
		return task_queue_.empty() && active_tasks_ == 0;
	}

	std::shared_ptr<cancellation_token> create_cancellation_token()
	{
		return std::make_shared<cancellation_token>();
	}

private:
	void worker_thread()
	{
		while (true)
		{
			std::function<void()> task;

			{
				std::unique_lock<std::mutex> lock(queue_mutex_);

				// Wait for task or shutdown
				condition_.wait(lock, [this] { return shutdown_ || !task_queue_.empty(); });

				if (shutdown_ && task_queue_.empty())
				{
					break;
				}

				if (!task_queue_.empty())
				{
					task = std::move(task_queue_.front());
					task_queue_.pop();
					active_tasks_++;
				}
			}

			if (task)
			{
				try
				{
					task();
				}
				catch (...)
				{
					// Swallow exceptions to keep worker alive
				}

				{
					std::unique_lock<std::mutex> lock(queue_mutex_);
					active_tasks_--;
					if (task_queue_.empty() && active_tasks_ == 0)
					{
						completion_cv_.notify_all();
					}
				}
			}
		}
	}

	const db_thread_config& config_;
	bool initialized_;
	bool shutdown_;

	mutable std::mutex queue_mutex_;
	std::condition_variable condition_;
	std::condition_variable completion_cv_;
	std::queue<std::function<void()>> task_queue_;
	std::vector<std::thread> workers_;
	std::size_t active_tasks_;
};

#endif

// ═══════════════════════════════════════════════════════════════
// Public Interface Implementation
// ═══════════════════════════════════════════════════════════════

thread_adapter::thread_adapter(const db_thread_config& config)
	: pimpl_(std::make_unique<impl>(config))
{
}

thread_adapter::~thread_adapter() = default;

thread_adapter::thread_adapter(thread_adapter&&) noexcept = default;
thread_adapter& thread_adapter::operator=(thread_adapter&&) noexcept = default;

common::VoidResult thread_adapter::initialize()
{
	return pimpl_->initialize();
}

common::VoidResult thread_adapter::shutdown()
{
	return pimpl_->shutdown();
}

bool thread_adapter::is_initialized() const
{
	return pimpl_->is_initialized();
}

common::VoidResult thread_adapter::execute(std::function<void()> task)
{
	return pimpl_->execute(std::move(task));
}

void thread_adapter::wait_for_completion()
{
	pimpl_->wait_for_completion();
}

bool thread_adapter::wait_for_completion_timeout(std::chrono::milliseconds timeout)
{
	return pimpl_->wait_for_completion_timeout(timeout);
}

std::size_t thread_adapter::worker_count() const
{
	return pimpl_->worker_count();
}

std::size_t thread_adapter::queue_size() const
{
	return pimpl_->queue_size();
}

bool thread_adapter::is_idle() const
{
	return pimpl_->is_idle();
}

std::shared_ptr<cancellation_token> thread_adapter::create_cancellation_token()
{
	return pimpl_->create_cancellation_token();
}

} // namespace adapters
} // namespace integrated
} // namespace database
