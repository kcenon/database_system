// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/database_types.h>
#include <kcenon/database/core/database_backend.h>
#include <kcenon/database/core/concepts.h>
#include <kcenon/database/adapters/thread_pool_adapter.h>
#include <future>
#include <memory>
#include <stdexcept>
#ifdef HAS_COROUTINES
#include <coroutine>
#endif
#include <functional>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <string>
#include <exception>
#include <vector>
#include <unordered_map>

#ifdef USE_THREAD_SYSTEM
    #include <kcenon/thread/core/job.h>
    #include <kcenon/thread/core/thread_worker.h>
    #include <kcenon/thread/interfaces/thread_context.h>
    #include <kcenon/thread/core/error_handling.h>
#endif

namespace database::async
{
#ifdef USE_THREAD_SYSTEM
	/**
	 * @class lambda_job
	 * @brief Wrapper to convert std::function into thread_system job
	 *
	 * This internal class adapts lambda/function objects to the thread_system
	 * job interface by overriding do_work().
	 */
	class lambda_job : public kcenon::thread::job {
	public:
		explicit lambda_job(std::function<void()> func, const std::string& name = "lambda_job")
			: job(name), func_(std::move(func)) {}

		kcenon::common::VoidResult do_work() override {
			try {
				if (func_) {
					func_();
				}
				return kcenon::common::ok();
			} catch (const std::exception& e) {
				return kcenon::common::error_info{
					static_cast<int>(kcenon::thread::error_code::job_execution_failed),
					std::string("Exception in lambda_job: ") + e.what(),
					"async_executor"
				};
			} catch (...) {
				return kcenon::common::error_info{
					static_cast<int>(kcenon::thread::error_code::job_execution_failed),
					"Unknown exception in lambda_job",
					"async_executor"
				};
			}
		}

	private:
		std::function<void()> func_;
	};
#endif

	// Forward declarations
	template<typename T> class async_result;
	class async_executor;
	class transaction_coordinator;
	class saga_builder;

	/**
	 * @class async_result
	 * @brief Template class for asynchronous operation results.
	 *
	 * Thread-safety: Callback registration methods (then, on_error) are thread-safe.
	 * get() and get_for() should only be called once as they consume the future.
	 */
	template<typename T>
	class async_result
	{
	public:
		async_result(std::future<T> future);

		// Blocking operations
		T get();
		T get_for(std::chrono::milliseconds timeout);

		// Non-blocking operations
		bool is_ready() const;
		std::future_status wait_for(std::chrono::milliseconds timeout) const;

		// Callback support - thread-safe
		// Uses C++20 concepts for type safety
		template<concepts::VoidCallable<T> Callback>
		void then(Callback&& callback);

		template<concepts::ErrorHandler Handler>
		void on_error(Handler&& error_handler);

		// Legacy overloads for backward compatibility
		void then(std::function<void(T)> callback);
		void on_error(std::function<void(const std::exception&)> error_handler);

	private:
		std::future<T> future_;
		mutable std::mutex callback_mutex_;
		std::function<void(T)> success_callback_;
		std::function<void(const std::exception&)> error_callback_;
	};

	// ── async_result<T> template method implementations ──────────────────

	template<typename T>
	async_result<T>::async_result(std::future<T> future)
		: future_(std::move(future))
	{
	}

	template<typename T>
	T async_result<T>::get()
	{
		std::function<void(T)> on_success;
		std::function<void(const std::exception&)> on_error_cb;
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			on_success = success_callback_;
			on_error_cb = error_callback_;
		}

		try {
			T value = future_.get();
			if (on_success) {
				on_success(value);
			}
			return value;
		} catch (const std::exception& e) {
			if (on_error_cb) {
				on_error_cb(e);
			}
			throw;
		}
	}

	template<typename T>
	T async_result<T>::get_for(std::chrono::milliseconds timeout)
	{
		auto status = future_.wait_for(timeout);
		if (status == std::future_status::timeout) {
			throw std::runtime_error("async_result::get_for timed out");
		}
		return get();
	}

	template<typename T>
	bool async_result<T>::is_ready() const
	{
		return future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
	}

	template<typename T>
	std::future_status async_result<T>::wait_for(std::chrono::milliseconds timeout) const
	{
		return future_.wait_for(timeout);
	}

	template<typename T>
	template<concepts::VoidCallable<T> Callback>
	void async_result<T>::then(Callback&& callback)
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		success_callback_ = std::forward<Callback>(callback);
	}

	template<typename T>
	template<concepts::ErrorHandler Handler>
	void async_result<T>::on_error(Handler&& error_handler)
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		error_callback_ = std::forward<Handler>(error_handler);
	}

	template<typename T>
	void async_result<T>::then(std::function<void(T)> callback)
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		success_callback_ = std::move(callback);
	}

	template<typename T>
	void async_result<T>::on_error(std::function<void(const std::exception&)> error_handler)
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		error_callback_ = std::move(error_handler);
	}

	// ── end async_result<T> implementations ──────────────────────────────

#ifdef HAS_COROUTINES
	/**
	 * @class coroutine_support
	 * @brief C++20 coroutine support for database operations.
	 * @note Requires C++20 coroutines. Only available when HAS_COROUTINES is defined.
	 */
	template<typename T>
	class database_awaitable
	{
	public:
		struct promise_type
		{
			T result_;
			std::exception_ptr exception_;

			database_awaitable get_return_object() {
				return database_awaitable{std::coroutine_handle<promise_type>::from_promise(*this)};
			}

			std::suspend_never initial_suspend() { return {}; }
			std::suspend_never final_suspend() noexcept { return {}; }

			void return_value(T value) { result_ = std::move(value); }
			void unhandled_exception() { exception_ = std::current_exception(); }
		};

		database_awaitable(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
		~database_awaitable() { if (handle_) handle_.destroy(); }

		database_awaitable(const database_awaitable&) = delete;
		database_awaitable& operator=(const database_awaitable&) = delete;

		database_awaitable(database_awaitable&& other) noexcept : handle_(other.handle_) {
			other.handle_ = nullptr;
		}

		database_awaitable& operator=(database_awaitable&& other) noexcept {
			if (this != &other) {
				if (handle_) handle_.destroy();
				handle_ = other.handle_;
				other.handle_ = nullptr;
			}
			return *this;
		}

		bool await_ready() const { return handle_.done(); }
		void await_suspend(std::coroutine_handle<> waiting_coroutine) {
			// Resume waiting coroutine when this one completes
		}

		T await_resume() {
			if (handle_.promise().exception_) {
				std::rethrow_exception(handle_.promise().exception_);
			}
			return std::move(handle_.promise().result_);
		}

	private:
		std::coroutine_handle<promise_type> handle_;
	};
#endif // HAS_COROUTINES

	/**
	 * @class async_database
	 * @brief Asynchronous database interface wrapper.
	 */
	class async_database
	{
	public:
		async_database(std::shared_ptr<core::database_backend> db, std::shared_ptr<async_executor> executor);

		// Asynchronous query operations
		async_result<bool> execute_async(const std::string& query);
		async_result<core::database_result> select_async(const std::string& query);

#ifdef HAS_COROUTINES
		// Coroutine support (C++20 only)
		database_awaitable<bool> execute_coro(const std::string& query);
		database_awaitable<core::database_result> select_coro(const std::string& query);
#endif

		// Batch operations
		async_result<std::vector<bool>> execute_batch_async(const std::vector<std::string>& queries);
		async_result<std::vector<core::database_result>> select_batch_async(const std::vector<std::string>& queries);

		// Transaction support
		async_result<bool> begin_transaction_async();
		async_result<bool> commit_transaction_async();
		async_result<bool> rollback_transaction_async();

		// Connection management
		async_result<bool> connect_async(const std::string& connection_string);
		async_result<bool> disconnect_async();

	private:
		std::shared_ptr<core::database_backend> db_;
		std::shared_ptr<async_executor> executor_;
	};

	/**
	 * @class async_executor
	 * @brief High-performance asynchronous executor using thread_system
	 *
	 * This executor leverages thread_system's advanced features when available:
	 * - Adaptive job queue (mutex ↔ lock-free automatic switching)
	 * - Sub-microsecond latency (77ns job scheduling)
	 * - 1.16M+ jobs/second throughput
	 * - Integrated monitoring and logging
	 *
	 * When USE_THREAD_SYSTEM is not defined, falls back to std::thread implementation.
	 *
	 * ### Thread Safety
	 * All methods are thread-safe and can be called from multiple threads.
	 *
	 * ### Performance
	 * - **Throughput**: 1.16M+ jobs/s (vs ~50K with std::thread)
	 * - **Latency**: 77ns scheduling (vs 2-5μs with std::thread)
	 * - **Scalability**: Linear scaling up to hardware concurrency
	 */
	class async_executor
	{
	public:
		/**
		 * @brief Constructs an async executor with specified thread count
		 * @param thread_count Number of worker threads (defaults to hardware concurrency)
		 * @param context Thread context for logging/monitoring (optional)
		 */
#ifdef USE_THREAD_SYSTEM
		explicit async_executor(
			size_t thread_count = std::thread::hardware_concurrency(),
			const thread_context_type& context = thread_context_type())
			: pool_(std::make_shared<thread_pool_type>("db_async_executor", context))
			, thread_count_(thread_count)
		{
			auto job_queue = pool_->get_job_queue();
			for (size_t i = 0; i < thread_count_; ++i) {
				auto worker = std::make_unique<kcenon::thread::thread_worker>(true, context);
				worker->set_job_queue(job_queue);

				auto add_result = pool_->enqueue(std::move(worker));
				if (add_result.is_err()) {
					throw std::runtime_error("Failed to add worker: " +
						add_result.error().message);
				}
			}

			auto result = pool_->start();
			if (result.is_err()) {
				throw std::runtime_error("Failed to start async executor: " +
					result.error().message);
			}
		}
#else
		explicit async_executor(
			size_t thread_count = std::thread::hardware_concurrency(),
			const thread_context_type& = thread_context_type())
			: thread_count_(thread_count)
			, stop_(false)
		{
			workers_.reserve(thread_count_);
			for (size_t i = 0; i < thread_count_; ++i) {
				workers_.emplace_back([this] { worker_thread(); });
			}
		}
#endif

		~async_executor() {
			shutdown();
		}

		// Prevent copying and moving
		async_executor(const async_executor&) = delete;
		async_executor& operator=(const async_executor&) = delete;
		async_executor(async_executor&&) = delete;
		async_executor& operator=(async_executor&&) = delete;

		/**
		 * @brief Submits a task for asynchronous execution
		 * @tparam F Callable type - constrained by SubmittableTask concept
		 * @tparam Args Argument types
		 * @param func The callable to execute
		 * @param args Arguments to pass to the callable
		 * @return std::future with the result of the callable
		 */
		template<typename F, typename... Args>
			requires concepts::SubmittableTask<F, Args...>
		auto submit(F&& func, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
		{
			using return_type = std::invoke_result_t<F, Args...>;

#ifdef USE_THREAD_SYSTEM
			auto task = std::make_shared<std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(func), std::forward<Args>(args)...)
			);

			auto future = task->get_future();

			auto job = std::make_unique<lambda_job>(
				[task]() { (*task)(); },
				"async_task"
			);

			auto result = pool_->enqueue(std::move(job));
			if (result.is_err()) {
				throw std::runtime_error("Failed to enqueue job: " +
					result.error().message);
			}

			return future;
#else
			auto task = std::make_shared<std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(func), std::forward<Args>(args)...)
			);

			auto future = task->get_future();

			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				if (stop_) {
					throw std::runtime_error("Cannot submit task to stopped executor");
				}
				tasks_.emplace([task]() { (*task)(); });
			}

			condition_.notify_one();
			return future;
#endif
		}

		/**
		 * @brief Gracefully shuts down the executor
		 */
		void shutdown() {
#ifdef USE_THREAD_SYSTEM
			if (pool_) {
				pool_->stop(false);
			}
#else
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				stop_ = true;
			}
			condition_.notify_all();

			for (auto& worker : workers_) {
				if (worker.joinable()) {
					worker.join();
				}
			}
			workers_.clear();
#endif
		}

		/**
		 * @brief Waits for all pending tasks to complete
		 */
		void wait_for_completion() {
#ifdef USE_THREAD_SYSTEM
			if (pool_) {
				while (pool_->get_job_queue()->size() > 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}
#else
			while (true) {
				{
					std::unique_lock<std::mutex> lock(queue_mutex_);
					if (tasks_.empty()) {
						break;
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
#endif
		}

		/**
		 * @brief Returns the number of pending tasks
		 */
		size_t pending_tasks() const {
#ifdef USE_THREAD_SYSTEM
			if (pool_) {
				return pool_->get_job_queue()->size();
			}
			return 0;
#else
			std::unique_lock<std::mutex> lock(queue_mutex_);
			return tasks_.size();
#endif
		}

		/**
		 * @brief Returns the number of worker threads
		 */
		size_t thread_count() const {
			return thread_count_;
		}

		/**
		 * @brief Checks if using thread_system implementation
		 */
		constexpr bool is_using_thread_system() const {
			return using_thread_system;
		}

	private:
#ifdef USE_THREAD_SYSTEM
		std::shared_ptr<thread_pool_type> pool_;
		size_t thread_count_;
#else
		void worker_thread() {
			while (true) {
				std::function<void()> task;

				{
					std::unique_lock<std::mutex> lock(queue_mutex_);
					condition_.wait(lock, [this] {
						return stop_ || !tasks_.empty();
					});

					if (stop_ && tasks_.empty()) {
						return;
					}

					if (!tasks_.empty()) {
						task = std::move(tasks_.front());
						tasks_.pop();
					}
				}

				if (task) {
					task();
				}
			}
		}

		std::vector<std::thread> workers_;
		std::queue<std::function<void()>> tasks_;
		mutable std::mutex queue_mutex_;
		std::condition_variable condition_;
		std::atomic<bool> stop_;
		size_t thread_count_;
#endif
	};

	/**
	 * @class stream_processor
	 * @brief Real-time data stream processing.
	 *
	 * Thread-safety: All public methods are thread-safe. Event handlers are called
	 * from dedicated stream threads, so handlers must be thread-safe if they access
	 * shared state. The handlers_mutex_ protects all handler registration and
	 * filter operations.
	 */
	class stream_processor
	{
	public:
		enum class stream_type {
			postgresql_notify,
			mongodb_change_stream,
			redis_pubsub,
			custom
		};

		struct stream_event {
			stream_type type;
			std::string channel;
			std::string payload;
			std::chrono::system_clock::time_point timestamp;
			std::unordered_map<std::string, std::string> metadata;
		};

		stream_processor(std::shared_ptr<core::database_backend> db);
		~stream_processor();

		// Stream management - thread-safe
		bool start_stream(stream_type type, const std::string& channel);
		bool stop_stream(const std::string& channel);
		void stop_all_streams();

		// Event handling - thread-safe with C++20 concepts
		// Defined inline to avoid MSVC C2244 with out-of-class concept constraints
		template<concepts::StreamEventHandler<stream_event> Handler>
		void register_event_handler(const std::string& channel, Handler&& handler)
		{
			std::lock_guard<std::mutex> lock(handlers_mutex_);
			event_handlers_[channel] = std::forward<Handler>(handler);
		}

		template<concepts::StreamEventHandler<stream_event> Handler>
		void register_global_handler(Handler&& handler)
		{
			std::lock_guard<std::mutex> lock(handlers_mutex_);
			global_handlers_.push_back(std::forward<Handler>(handler));
		}

		// Legacy overloads for backward compatibility
		void register_event_handler(const std::string& channel,
		                           std::function<void(const stream_event&)> handler);
		void register_global_handler(std::function<void(const stream_event&)> handler);

		// Filter support - thread-safe with C++20 concepts
		// Defined inline to avoid MSVC C2244 with out-of-class concept constraints
		template<concepts::StreamEventFilter<stream_event> Filter>
		void add_event_filter(const std::string& channel, Filter&& filter)
		{
			std::lock_guard<std::mutex> lock(handlers_mutex_);
			event_filters_[channel] = std::forward<Filter>(filter);
		}

		// Legacy overload for backward compatibility
		void add_event_filter(const std::string& channel,
		                     std::function<bool(const stream_event&)> filter);

	private:
		void stream_thread(const std::string& channel, stream_type type);
		void process_event(const stream_event& event);

		std::shared_ptr<core::database_backend> db_;
		std::mutex threads_mutex_;  // Protects stream_threads_
		std::unordered_map<std::string, std::thread> stream_threads_;
		std::mutex handlers_mutex_;  // Protects all handler/filter containers
		std::unordered_map<std::string, std::function<void(const stream_event&)>> event_handlers_;
		std::vector<std::function<void(const stream_event&)>> global_handlers_;
		std::unordered_map<std::string, std::function<bool(const stream_event&)>> event_filters_;
		std::atomic<bool> running_{true};
	};

	/**
	 * @class transaction_coordinator
	 * @brief Distributed transaction coordination.
	 *
	 * @note This class uses dependency injection pattern.
	 * Access via database_context::get_transaction_coordinator() (Sprint 3, Task 3.1).
	 *
	 * @example
	 * @code
	 * auto context = std::make_shared<database_context>();
	 * auto txn_coord = context->get_transaction_coordinator();
	 * @endcode
	 *
	 * Thread-safety: All public methods are thread-safe through transactions_mutex_ protection.
	 */
	class transaction_coordinator
	{
	public:
		enum class transaction_state {
			active,
			preparing,
			prepared,
			committing,
			committed,
			aborting,
			aborted
		};

		struct distributed_transaction {
			std::string transaction_id;
			std::vector<std::shared_ptr<core::database_backend>> participants;
			transaction_state state;
			std::chrono::system_clock::time_point start_time;
			std::chrono::system_clock::time_point last_activity;
		};

		/**
		 * @brief Default constructor - used by database_context
		 */
		transaction_coordinator() = default;

		// Transaction management
		std::string begin_distributed_transaction(const std::vector<std::shared_ptr<core::database_backend>>& participants);
		async_result<bool> commit_distributed_transaction(const std::string& transaction_id);
		async_result<bool> rollback_distributed_transaction(const std::string& transaction_id);

		// Two-phase commit protocol
		async_result<bool> prepare_phase(const std::string& transaction_id);
		async_result<bool> commit_phase(const std::string& transaction_id);

		// Saga pattern support
		saga_builder create_saga();

		// Transaction recovery
		void recover_transactions();
		std::vector<distributed_transaction> get_active_transactions() const;

	private:
		async_result<bool> two_phase_commit(const std::string& transaction_id);
		void cleanup_completed_transactions();

		mutable std::mutex transactions_mutex_;
		std::unordered_map<std::string, distributed_transaction> active_transactions_;
		std::shared_ptr<async_executor> executor_;
	};

	/**
	 * @class saga_builder
	 * @brief Builder for Saga pattern transactions.
	 */
	class saga_builder
	{
	public:
		saga_builder(transaction_coordinator& coordinator);

		// Saga step definition with C++20 concepts
		template<concepts::TransactionAction Action, concepts::CompensationAction Compensation>
		saga_builder& add_step(Action&& action, Compensation&& compensation);

		// Legacy overload for backward compatibility
		saga_builder& add_step(std::function<async_result<bool>()> action,
		                      std::function<async_result<bool>()> compensation);

		// Execution
		async_result<bool> execute();

	private:
		struct saga_step {
			std::function<async_result<bool>()> action;
			std::function<async_result<bool>()> compensation;
		};

		transaction_coordinator& coordinator_;
		std::vector<saga_step> steps_;
	};

	// ── async_database inline implementations ───────────────────────────

	inline async_database::async_database(
		std::shared_ptr<core::database_backend> db,
		std::shared_ptr<async_executor> executor)
		: db_(std::move(db))
		, executor_(std::move(executor))
	{
	}

	inline async_result<bool> async_database::execute_async(const std::string& query)
	{
		auto db = db_;
		auto future = executor_->submit([db, query]() -> bool {
			auto result = db->execute_query(query);
			if (result.is_err()) {
				throw std::runtime_error(result.error().message);
			}
			return true;
		});
		return async_result<bool>(std::move(future));
	}

	inline async_result<core::database_result> async_database::select_async(const std::string& query)
	{
		auto db = db_;
		auto future = executor_->submit([db, query]() -> core::database_result {
			auto result = db->select_query(query);
			if (result.is_err()) {
				throw std::runtime_error(result.error().message);
			}
			return result.value();
		});
		return async_result<core::database_result>(std::move(future));
	}

	inline async_result<std::vector<bool>> async_database::execute_batch_async(
		const std::vector<std::string>& queries)
	{
		auto db = db_;
		auto queries_copy = queries;
		auto future = executor_->submit([db, queries_copy]() -> std::vector<bool> {
			std::vector<bool> results;
			results.reserve(queries_copy.size());
			for (const auto& q : queries_copy) {
				auto result = db->execute_query(q);
				results.push_back(result.is_ok());
			}
			return results;
		});
		return async_result<std::vector<bool>>(std::move(future));
	}

	inline async_result<std::vector<core::database_result>> async_database::select_batch_async(
		const std::vector<std::string>& queries)
	{
		auto db = db_;
		auto queries_copy = queries;
		auto future = executor_->submit([db, queries_copy]() -> std::vector<core::database_result> {
			std::vector<core::database_result> results;
			results.reserve(queries_copy.size());
			for (const auto& q : queries_copy) {
				auto result = db->select_query(q);
				if (result.is_ok()) {
					results.push_back(result.value());
				} else {
					results.push_back(core::database_result{});
				}
			}
			return results;
		});
		return async_result<std::vector<core::database_result>>(std::move(future));
	}

	inline async_result<bool> async_database::begin_transaction_async()
	{
		auto db = db_;
		auto future = executor_->submit([db]() -> bool {
			auto result = db->begin_transaction();
			if (result.is_err()) {
				throw std::runtime_error(result.error().message);
			}
			return true;
		});
		return async_result<bool>(std::move(future));
	}

	inline async_result<bool> async_database::commit_transaction_async()
	{
		auto db = db_;
		auto future = executor_->submit([db]() -> bool {
			auto result = db->commit_transaction();
			if (result.is_err()) {
				throw std::runtime_error(result.error().message);
			}
			return true;
		});
		return async_result<bool>(std::move(future));
	}

	inline async_result<bool> async_database::rollback_transaction_async()
	{
		auto db = db_;
		auto future = executor_->submit([db]() -> bool {
			auto result = db->rollback_transaction();
			if (result.is_err()) {
				throw std::runtime_error(result.error().message);
			}
			return true;
		});
		return async_result<bool>(std::move(future));
	}

	inline async_result<bool> async_database::connect_async(const std::string& connection_string)
	{
		auto db = db_;
		auto future = executor_->submit([db, connection_string]() -> bool {
			auto config = core::connection_config::from_string(connection_string);
			auto result = db->initialize(config);
			if (result.is_err()) {
				throw std::runtime_error(result.error().message);
			}
			return true;
		});
		return async_result<bool>(std::move(future));
	}

	inline async_result<bool> async_database::disconnect_async()
	{
		auto db = db_;
		auto future = executor_->submit([db]() -> bool {
			auto result = db->shutdown();
			if (result.is_err()) {
				throw std::runtime_error(result.error().message);
			}
			return true;
		});
		return async_result<bool>(std::move(future));
	}

	// ── end async_database implementations ───────────────────────────────

	// Helper functions for async operations
	template<typename T>
	async_result<T> make_ready_result(T value) {
		std::promise<T> promise;
		promise.set_value(std::move(value));
		return async_result<T>(promise.get_future());
	}

	template<typename T>
	async_result<T> make_error_result(const std::exception& error) {
		std::promise<T> promise;
		promise.set_exception(std::make_exception_ptr(error));
		return async_result<T>(promise.get_future());
	}

	// ── transaction_coordinator inline implementations ───────────────────

	inline std::string transaction_coordinator::begin_distributed_transaction(
		const std::vector<std::shared_ptr<core::database_backend>>& participants)
	{
		static std::atomic<uint64_t> id_counter{0};
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		std::string txn_id = "txn-" + std::to_string(ms) + "-"
			+ std::to_string(id_counter.fetch_add(1));

		std::lock_guard<std::mutex> lock(transactions_mutex_);
		distributed_transaction txn;
		txn.transaction_id = txn_id;
		txn.participants = participants;
		txn.state = transaction_state::active;
		txn.start_time = std::chrono::system_clock::now();
		txn.last_activity = txn.start_time;
		active_transactions_[txn_id] = std::move(txn);
		return txn_id;
	}

	inline async_result<bool> transaction_coordinator::prepare_phase(
		const std::string& transaction_id)
	{
		std::vector<std::shared_ptr<core::database_backend>> participants;
		{
			std::lock_guard<std::mutex> lock(transactions_mutex_);
			auto it = active_transactions_.find(transaction_id);
			if (it == active_transactions_.end()) {
				return make_error_result<bool>(
					std::runtime_error("Transaction not found: " + transaction_id));
			}
			it->second.state = transaction_state::preparing;
			it->second.last_activity = std::chrono::system_clock::now();
			participants = it->second.participants;
		}

		std::vector<std::shared_ptr<core::database_backend>> prepared;
		for (const auto& participant : participants) {
			auto result = participant->begin_transaction();
			if (result.is_err()) {
				for (const auto& p : prepared) {
					p->rollback_transaction();
				}
				std::lock_guard<std::mutex> lock(transactions_mutex_);
				auto it = active_transactions_.find(transaction_id);
				if (it != active_transactions_.end()) {
					it->second.state = transaction_state::aborted;
				}
				return make_ready_result(false);
			}
			prepared.push_back(participant);
		}

		{
			std::lock_guard<std::mutex> lock(transactions_mutex_);
			auto it = active_transactions_.find(transaction_id);
			if (it != active_transactions_.end()) {
				it->second.state = transaction_state::prepared;
			}
		}
		return make_ready_result(true);
	}

	inline async_result<bool> transaction_coordinator::commit_phase(
		const std::string& transaction_id)
	{
		std::vector<std::shared_ptr<core::database_backend>> participants;
		{
			std::lock_guard<std::mutex> lock(transactions_mutex_);
			auto it = active_transactions_.find(transaction_id);
			if (it == active_transactions_.end()) {
				return make_error_result<bool>(
					std::runtime_error("Transaction not found: " + transaction_id));
			}
			if (it->second.state != transaction_state::prepared) {
				return make_ready_result(false);
			}
			it->second.state = transaction_state::committing;
			it->second.last_activity = std::chrono::system_clock::now();
			participants = it->second.participants;
		}

		bool all_committed = true;
		for (const auto& participant : participants) {
			auto result = participant->commit_transaction();
			if (result.is_err()) {
				all_committed = false;
			}
		}

		{
			std::lock_guard<std::mutex> lock(transactions_mutex_);
			auto it = active_transactions_.find(transaction_id);
			if (it != active_transactions_.end()) {
				it->second.state = all_committed
					? transaction_state::committed
					: transaction_state::aborted;
			}
		}
		return make_ready_result(all_committed);
	}

	inline async_result<bool> transaction_coordinator::commit_distributed_transaction(
		const std::string& transaction_id)
	{
		return two_phase_commit(transaction_id);
	}

	inline async_result<bool> transaction_coordinator::rollback_distributed_transaction(
		const std::string& transaction_id)
	{
		std::vector<std::shared_ptr<core::database_backend>> participants;
		{
			std::lock_guard<std::mutex> lock(transactions_mutex_);
			auto it = active_transactions_.find(transaction_id);
			if (it == active_transactions_.end()) {
				return make_error_result<bool>(
					std::runtime_error("Transaction not found: " + transaction_id));
			}
			it->second.state = transaction_state::aborting;
			it->second.last_activity = std::chrono::system_clock::now();
			participants = it->second.participants;
		}

		bool all_rolled_back = true;
		for (const auto& participant : participants) {
			auto result = participant->rollback_transaction();
			if (result.is_err()) {
				all_rolled_back = false;
			}
		}

		{
			std::lock_guard<std::mutex> lock(transactions_mutex_);
			auto it = active_transactions_.find(transaction_id);
			if (it != active_transactions_.end()) {
				it->second.state = transaction_state::aborted;
			}
		}
		return make_ready_result(all_rolled_back);
	}

	inline async_result<bool> transaction_coordinator::two_phase_commit(
		const std::string& transaction_id)
	{
		bool prepared = prepare_phase(transaction_id).get();
		if (!prepared) {
			return make_ready_result(false);
		}
		return commit_phase(transaction_id);
	}

	inline void transaction_coordinator::recover_transactions()
	{
		cleanup_completed_transactions();
	}

	inline std::vector<transaction_coordinator::distributed_transaction>
	transaction_coordinator::get_active_transactions() const
	{
		std::lock_guard<std::mutex> lock(transactions_mutex_);
		std::vector<distributed_transaction> result;
		result.reserve(active_transactions_.size());
		for (const auto& [id, txn] : active_transactions_) {
			result.push_back(txn);
		}
		return result;
	}

	inline saga_builder transaction_coordinator::create_saga()
	{
		return saga_builder(*this);
	}

	inline void transaction_coordinator::cleanup_completed_transactions()
	{
		std::lock_guard<std::mutex> lock(transactions_mutex_);
		for (auto it = active_transactions_.begin();
			 it != active_transactions_.end();) {
			if (it->second.state == transaction_state::committed
				|| it->second.state == transaction_state::aborted) {
				it = active_transactions_.erase(it);
			} else {
				++it;
			}
		}
	}

	// ── end transaction_coordinator implementations ──────────────────────

	// ── saga_builder inline implementations ──────────────────────────────

	inline saga_builder::saga_builder(transaction_coordinator& coordinator)
		: coordinator_(coordinator)
	{
	}

	inline saga_builder& saga_builder::add_step(
		std::function<async_result<bool>()> action,
		std::function<async_result<bool>()> compensation)
	{
		steps_.push_back({std::move(action), std::move(compensation)});
		return *this;
	}

	template<concepts::TransactionAction Action,
			 concepts::CompensationAction Compensation>
	saga_builder& saga_builder::add_step(
		Action&& action, Compensation&& compensation)
	{
		steps_.push_back({
			std::function<async_result<bool>()>(std::forward<Action>(action)),
			std::function<async_result<bool>()>(std::forward<Compensation>(compensation))
		});
		return *this;
	}

	inline async_result<bool> saga_builder::execute()
	{
		std::vector<size_t> completed;

		for (size_t i = 0; i < steps_.size(); ++i) {
			try {
				bool success = steps_[i].action().get();
				if (!success) {
					for (auto it = completed.rbegin();
						 it != completed.rend(); ++it) {
						try { steps_[*it].compensation().get(); }
						catch (...) {}
					}
					return make_ready_result(false);
				}
				completed.push_back(i);
			} catch (...) {
				for (auto it = completed.rbegin();
					 it != completed.rend(); ++it) {
					try { steps_[*it].compensation().get(); }
					catch (...) {}
				}
				return make_ready_result(false);
			}
		}

		return make_ready_result(true);
	}

	// ── end saga_builder implementations ─────────────────────────────────

	// ── stream_processor inline implementations ──────────────────────────

	inline stream_processor::stream_processor(
		std::shared_ptr<core::database_backend> db)
		: db_(std::move(db))
	{
	}

	inline stream_processor::~stream_processor()
	{
		stop_all_streams();
	}

	inline bool stream_processor::start_stream(
		stream_type type, const std::string& channel)
	{
		std::lock_guard<std::mutex> lock(threads_mutex_);
		if (stream_threads_.count(channel) > 0) {
			return false;
		}
		stream_threads_.emplace(
			channel,
			std::thread(&stream_processor::stream_thread, this, channel, type));
		return true;
	}

	inline bool stream_processor::stop_stream(const std::string& channel)
	{
		std::thread t;
		{
			std::lock_guard<std::mutex> lock(threads_mutex_);
			auto it = stream_threads_.find(channel);
			if (it == stream_threads_.end()) {
				return false;
			}
			t = std::move(it->second);
			stream_threads_.erase(it);
		}
		// Thread detects its channel removal via map-check and exits
		if (t.joinable()) {
			t.join();
		}
		return true;
	}

	inline void stream_processor::stop_all_streams()
	{
		running_.store(false);
		std::unordered_map<std::string, std::thread> threads;
		{
			std::lock_guard<std::mutex> lock(threads_mutex_);
			threads.swap(stream_threads_);
		}
		for (auto& [channel, t] : threads) {
			if (t.joinable()) {
				t.join();
			}
		}
		running_.store(true);
	}

	inline void stream_processor::register_event_handler(
		const std::string& channel,
		std::function<void(const stream_event&)> handler)
	{
		std::lock_guard<std::mutex> lock(handlers_mutex_);
		event_handlers_[channel] = std::move(handler);
	}

	inline void stream_processor::register_global_handler(
		std::function<void(const stream_event&)> handler)
	{
		std::lock_guard<std::mutex> lock(handlers_mutex_);
		global_handlers_.push_back(std::move(handler));
	}

	inline void stream_processor::add_event_filter(
		const std::string& channel,
		std::function<bool(const stream_event&)> filter)
	{
		std::lock_guard<std::mutex> lock(handlers_mutex_);
		event_filters_[channel] = std::move(filter);
	}

	inline void stream_processor::stream_thread(
		const std::string& channel, stream_type type)
	{
		stream_event connected_event;
		connected_event.type = type;
		connected_event.channel = channel;
		connected_event.payload = "stream_connected";
		connected_event.timestamp = std::chrono::system_clock::now();
		process_event(connected_event);

		while (running_.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			{
				std::lock_guard<std::mutex> lock(threads_mutex_);
				if (stream_threads_.find(channel) == stream_threads_.end()) {
					return;
				}
			}
		}
	}

	inline void stream_processor::process_event(const stream_event& event)
	{
		std::lock_guard<std::mutex> lock(handlers_mutex_);

		auto filter_it = event_filters_.find(event.channel);
		if (filter_it != event_filters_.end()) {
			if (!filter_it->second(event)) {
				return;
			}
		}

		auto handler_it = event_handlers_.find(event.channel);
		if (handler_it != event_handlers_.end()) {
			handler_it->second(event);
		}

		for (const auto& handler : global_handlers_) {
			handler(event);
		}
	}

	// ── end stream_processor implementations ─────────────────────────────

	// Coroutine helpers (C++20 only)
#ifdef HAS_COROUTINES
	inline auto when_all(std::vector<database_awaitable<bool>> awaitables) -> database_awaitable<std::vector<bool>> {
		std::vector<bool> results;
		for (auto& awaitable : awaitables) {
			results.push_back(co_await awaitable);
		}
		co_return results;
	}

	inline auto when_any(std::vector<database_awaitable<bool>> awaitables) -> database_awaitable<bool> {
		// In a real implementation, this would race the awaitables
		if (!awaitables.empty()) {
			co_return co_await awaitables[0];
		}
		co_return false;
	}
#endif // HAS_COROUTINES

} // namespace database::async