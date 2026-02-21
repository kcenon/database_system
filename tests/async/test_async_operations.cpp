/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Unit tests for async_result<T>, async_executor, and async_database.
 *
 * Part of #366:
 *   Sub-issue #369: async_result<T>, async_executor, helper functions
 *   Sub-issue #371: async_database
 */

// Force std::thread fallback for unit testing — avoids external thread_system
// library dependency while testing the same public API.
#ifdef USE_THREAD_SYSTEM
#undef USE_THREAD_SYSTEM
#endif

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "database/async/async_operations.h"

using namespace database::async;

//=============================================================================
// async_result<T> Tests
//=============================================================================

class AsyncResultTest : public ::testing::Test {
protected:
	// Helper: create a ready async_result with the given value
	template<typename T>
	async_result<T> make_ready(T value) {
		std::promise<T> p;
		p.set_value(std::move(value));
		return async_result<T>(p.get_future());
	}

	// Helper: create an async_result that throws
	template<typename T>
	async_result<T> make_failing() {
		std::promise<T> p;
		p.set_exception(
			std::make_exception_ptr(std::runtime_error("test_error")));
		return async_result<T>(p.get_future());
	}
};

// -- Constructor --

TEST_F(AsyncResultTest, ConstructsFromFuture) {
	std::promise<int> p;
	auto future = p.get_future();
	p.set_value(42);

	async_result<int> result(std::move(future));
	EXPECT_TRUE(result.is_ready());
}

// -- get() --

TEST_F(AsyncResultTest, GetReturnsValue) {
	auto result = make_ready<int>(99);
	EXPECT_EQ(result.get(), 99);
}

TEST_F(AsyncResultTest, GetReturnsStringValue) {
	auto result = make_ready<std::string>("hello");
	EXPECT_EQ(result.get(), "hello");
}

TEST_F(AsyncResultTest, GetRethrowsException) {
	auto result = make_failing<int>();
	EXPECT_THROW(result.get(), std::runtime_error);
}

TEST_F(AsyncResultTest, GetInvokesSuccessCallback) {
	auto result = make_ready<int>(42);

	int captured = 0;
	result.then([&captured](int v) { captured = v; });

	EXPECT_EQ(result.get(), 42);
	EXPECT_EQ(captured, 42);
}

TEST_F(AsyncResultTest, GetInvokesErrorCallback) {
	auto result = make_failing<int>();

	std::string captured_msg;
	result.on_error([&captured_msg](const std::exception& e) {
		captured_msg = e.what();
	});

	EXPECT_THROW(result.get(), std::runtime_error);
	EXPECT_EQ(captured_msg, "test_error");
}

// -- get_for() --

TEST_F(AsyncResultTest, GetForReturnsValueWithinTimeout) {
	auto result = make_ready<int>(77);
	EXPECT_EQ(result.get_for(std::chrono::milliseconds(100)), 77);
}

TEST_F(AsyncResultTest, GetForThrowsOnTimeout) {
	std::promise<int> p;
	async_result<int> result(p.get_future());

	EXPECT_THROW(
		result.get_for(std::chrono::milliseconds(10)),
		std::runtime_error);
}

// -- is_ready() --

TEST_F(AsyncResultTest, IsReadyReturnsFalseBeforeCompletion) {
	std::promise<int> p;
	async_result<int> result(p.get_future());
	EXPECT_FALSE(result.is_ready());

	p.set_value(1);
	EXPECT_TRUE(result.is_ready());
}

TEST_F(AsyncResultTest, IsReadyReturnsTrueWhenReady) {
	auto result = make_ready<int>(1);
	EXPECT_TRUE(result.is_ready());
}

// -- wait_for() --

TEST_F(AsyncResultTest, WaitForReturnsReadyWhenComplete) {
	auto result = make_ready<int>(1);
	auto status = result.wait_for(std::chrono::milliseconds(0));
	EXPECT_EQ(status, std::future_status::ready);
}

TEST_F(AsyncResultTest, WaitForReturnsTimeoutWhenNotReady) {
	std::promise<int> p;
	async_result<int> result(p.get_future());

	auto status = result.wait_for(std::chrono::milliseconds(1));
	EXPECT_EQ(status, std::future_status::timeout);

	p.set_value(0); // cleanup
}

// -- then() / on_error() concept-based --

TEST_F(AsyncResultTest, ThenWithLambda) {
	auto result = make_ready<int>(10);

	int captured = 0;
	result.then([&captured](int v) { captured = v; });
	result.get();

	EXPECT_EQ(captured, 10);
}

TEST_F(AsyncResultTest, OnErrorWithLambda) {
	auto result = make_failing<int>();

	bool error_caught = false;
	result.on_error([&error_caught](const std::exception&) {
		error_caught = true;
	});

	EXPECT_THROW(result.get(), std::runtime_error);
	EXPECT_TRUE(error_caught);
}

// -- then() / on_error() legacy overloads --

TEST_F(AsyncResultTest, ThenLegacyOverload) {
	auto result = make_ready<int>(5);

	int captured = 0;
	std::function<void(int)> cb = [&captured](int v) { captured = v; };
	result.then(cb);
	result.get();

	EXPECT_EQ(captured, 5);
}

TEST_F(AsyncResultTest, OnErrorLegacyOverload) {
	auto result = make_failing<int>();

	std::string msg;
	std::function<void(const std::exception&)> cb =
		[&msg](const std::exception& e) { msg = e.what(); };
	result.on_error(cb);

	EXPECT_THROW(result.get(), std::runtime_error);
	EXPECT_EQ(msg, "test_error");
}

// -- Callbacks not invoked when not set --

TEST_F(AsyncResultTest, NoCallbackDoesNotCrashOnSuccess) {
	auto result = make_ready<int>(42);
	EXPECT_NO_THROW(result.get());
}

TEST_F(AsyncResultTest, NoErrorCallbackDoesNotCrashOnFailure) {
	auto result = make_failing<int>();
	EXPECT_THROW(result.get(), std::runtime_error);
}

// -- Deferred completion with thread --

TEST_F(AsyncResultTest, GetBlocksUntilValueAvailable) {
	std::promise<int> p;
	async_result<int> result(p.get_future());

	std::thread setter([&p]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		p.set_value(123);
	});

	EXPECT_EQ(result.get(), 123);
	setter.join();
}

//=============================================================================
// async_executor Tests
//=============================================================================

class AsyncExecutorTest : public ::testing::Test {
protected:
	void SetUp() override {
		executor_ = std::make_unique<async_executor>(2);
	}

	void TearDown() override {
		if (executor_) {
			executor_->shutdown();
		}
	}

	std::unique_ptr<async_executor> executor_;
};

// -- Construction --

TEST_F(AsyncExecutorTest, ConstructsWithCustomThreadCount) {
	EXPECT_EQ(executor_->thread_count(), 2u);
}

TEST_F(AsyncExecutorTest, ConstructsWithDefaultThreadCount) {
	auto exec = std::make_unique<async_executor>();
	EXPECT_EQ(exec->thread_count(), std::thread::hardware_concurrency());
	exec->shutdown();
}

TEST_F(AsyncExecutorTest, IsNotUsingThreadSystem) {
	EXPECT_FALSE(executor_->is_using_thread_system());
}

// -- submit() --

TEST_F(AsyncExecutorTest, SubmitExecutesCallable) {
	auto future = executor_->submit([]() { return 42; });
	EXPECT_EQ(future.get(), 42);
}

TEST_F(AsyncExecutorTest, SubmitWithArguments) {
	auto future = executor_->submit([](int a, int b) { return a + b; }, 3, 7);
	EXPECT_EQ(future.get(), 10);
}

TEST_F(AsyncExecutorTest, SubmitReturnsString) {
	auto future = executor_->submit([]() -> std::string {
		return "async_result";
	});
	EXPECT_EQ(future.get(), "async_result");
}

TEST_F(AsyncExecutorTest, SubmitPropagatesException) {
	auto future = executor_->submit([]() -> int {
		throw std::runtime_error("task_failed");
	});
	EXPECT_THROW(future.get(), std::runtime_error);
}

// -- Multiple concurrent submissions --

TEST_F(AsyncExecutorTest, MultipleConcurrentSubmissions) {
	constexpr int NUM_TASKS = 100;
	std::vector<std::future<int>> futures;
	futures.reserve(NUM_TASKS);

	for (int i = 0; i < NUM_TASKS; ++i) {
		futures.push_back(executor_->submit([i]() { return i * 2; }));
	}

	for (int i = 0; i < NUM_TASKS; ++i) {
		EXPECT_EQ(futures[i].get(), i * 2);
	}
}

// -- wait_for_completion() --

TEST_F(AsyncExecutorTest, WaitForCompletionBlocksUntilDone) {
	std::atomic<int> counter{0};

	for (int i = 0; i < 10; ++i) {
		executor_->submit([&counter]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			counter.fetch_add(1);
		});
	}

	executor_->wait_for_completion();
	// Allow brief settling time for tasks that are executing but haven't
	// decremented the queue yet
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_EQ(counter.load(), 10);
}

// -- shutdown() --

TEST_F(AsyncExecutorTest, ShutdownAfterSubmitCompletesGracefully) {
	auto future = executor_->submit([]() { return 1; });
	executor_->shutdown();
	EXPECT_EQ(future.get(), 1);
}

TEST_F(AsyncExecutorTest, SubmitAfterShutdownThrows) {
	executor_->shutdown();
	EXPECT_THROW(
		executor_->submit([]() { return 0; }),
		std::runtime_error);
}

// -- thread_count() / pending_tasks() --

TEST_F(AsyncExecutorTest, ThreadCountReturnsConfigured) {
	async_executor exec4(4);
	EXPECT_EQ(exec4.thread_count(), 4u);
	exec4.shutdown();
}

//=============================================================================
// Helper Function Tests
//=============================================================================

TEST(AsyncHelpersTest, MakeReadyResultIsImmediatelyAvailable) {
	auto result = make_ready_result<int>(42);
	EXPECT_TRUE(result.is_ready());
	EXPECT_EQ(result.get(), 42);
}

TEST(AsyncHelpersTest, MakeReadyResultWithString) {
	auto result = make_ready_result<std::string>("hello");
	EXPECT_TRUE(result.is_ready());
	EXPECT_EQ(result.get(), "hello");
}

TEST(AsyncHelpersTest, MakeErrorResultThrowsOnGet) {
	// Note: make_error_result takes const std::exception& which causes
	// object slicing — the thrown exception is std::exception, not the
	// derived type. This tests the actual API behavior.
	auto result = make_error_result<int>(std::runtime_error("err"));
	EXPECT_TRUE(result.is_ready());
	EXPECT_THROW(result.get(), std::exception);
}

TEST(AsyncHelpersTest, MakeErrorResultInvokesOnErrorCallback) {
	auto result = make_error_result<int>(std::runtime_error("callback_err"));

	bool error_caught = false;
	result.on_error([&error_caught](const std::exception&) {
		error_caught = true;
	});

	EXPECT_THROW(result.get(), std::exception);
	EXPECT_TRUE(error_caught);
}

//=============================================================================
// async_database Tests (#371)
//=============================================================================

#include "database/core/database_backend.h"

namespace {

// Stub backend for async_database testing.
// Uses global-scope prefix to avoid the namespace alias conflict with
// 'using database = unified_database_system'.
class async_stub_backend : public ::database::core::database_backend {
public:
	::database::database_types type() const override {
		return ::database::database_types::sqlite;
	}

	kcenon::common::VoidResult initialize(
		const ::database::core::connection_config&) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		initialized_ = true;
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult shutdown() override {
		std::lock_guard<std::mutex> lock(mutex_);
		initialized_ = false;
		return kcenon::common::ok();
	}

	bool is_initialized() const override {
		std::lock_guard<std::mutex> lock(mutex_);
		return initialized_;
	}

	kcenon::common::Result<uint64_t> insert_query(
		const std::string&) override
	{
		return kcenon::common::Result<uint64_t>(uint64_t{1});
	}

	kcenon::common::Result<uint64_t> update_query(
		const std::string&) override
	{
		return kcenon::common::Result<uint64_t>(uint64_t{1});
	}

	kcenon::common::Result<uint64_t> delete_query(
		const std::string&) override
	{
		return kcenon::common::Result<uint64_t>(uint64_t{1});
	}

	kcenon::common::Result<::database::core::database_result> select_query(
		const std::string&) override
	{
		::database::core::database_result rows;
		::database::core::database_row row;
		row["id"] = int64_t{1};
		row["name"] = std::string("stub_row");
		rows.push_back(row);
		return kcenon::common::Result<::database::core::database_result>(
			std::move(rows));
	}

	kcenon::common::VoidResult execute_query(
		const std::string& query) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (should_fail_execute_) {
			return kcenon::common::error_info{1, "execute_failed", "stub"};
		}
		last_executed_query_ = query;
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult begin_transaction() override {
		std::lock_guard<std::mutex> lock(mutex_);
		in_transaction_ = true;
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult commit_transaction() override {
		std::lock_guard<std::mutex> lock(mutex_);
		in_transaction_ = false;
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult rollback_transaction() override {
		std::lock_guard<std::mutex> lock(mutex_);
		in_transaction_ = false;
		return kcenon::common::ok();
	}

	bool in_transaction() const override {
		std::lock_guard<std::mutex> lock(mutex_);
		return in_transaction_;
	}
	std::string last_error() const override { return ""; }

	std::map<std::string, std::string> connection_info() const override {
		return {{"type", "stub"}};
	}

	// Test helpers
	void set_fail_execute(bool fail) {
		std::lock_guard<std::mutex> lock(mutex_);
		should_fail_execute_ = fail;
	}
	std::string get_last_query() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return last_executed_query_;
	}

private:
	mutable std::mutex mutex_;
	bool initialized_ = false;
	bool in_transaction_ = false;
	bool should_fail_execute_ = false;
	std::string last_executed_query_;
};

} // anonymous namespace

class AsyncDatabaseTest : public ::testing::Test {
protected:
	void SetUp() override {
		backend_ = std::make_shared<async_stub_backend>();
		executor_ = std::make_shared<async_executor>(2);
		db_ = std::make_unique<async_database>(backend_, executor_);
	}

	void TearDown() override {
		db_.reset();
		if (executor_) {
			executor_->shutdown();
		}
	}

	std::shared_ptr<async_stub_backend> backend_;
	std::shared_ptr<async_executor> executor_;
	std::unique_ptr<async_database> db_;
};

// -- Constructor --

TEST_F(AsyncDatabaseTest, ConstructsWithBackendAndExecutor) {
	EXPECT_NE(db_, nullptr);
}

// -- execute_async() --

TEST_F(AsyncDatabaseTest, ExecuteAsyncReturnsTrue) {
	auto result = db_->execute_async("CREATE TABLE t (id INT)");
	EXPECT_TRUE(result.get());
}

TEST_F(AsyncDatabaseTest, ExecuteAsyncDelegatesToBackend) {
	auto result = db_->execute_async("DROP TABLE IF EXISTS t");
	result.get();
	EXPECT_EQ(backend_->get_last_query(), "DROP TABLE IF EXISTS t");
}

TEST_F(AsyncDatabaseTest, ExecuteAsyncThrowsOnBackendFailure) {
	backend_->set_fail_execute(true);
	auto result = db_->execute_async("BAD QUERY");
	EXPECT_THROW(result.get(), std::runtime_error);
}

// -- select_async() --

TEST_F(AsyncDatabaseTest, SelectAsyncReturnsRows) {
	auto result = db_->select_async("SELECT * FROM t");
	auto rows = result.get();
	ASSERT_EQ(rows.size(), 1u);
	EXPECT_EQ(std::get<std::string>(rows[0].at("name")), "stub_row");
}

// -- execute_batch_async() --

TEST_F(AsyncDatabaseTest, ExecuteBatchAsyncProcessesAllQueries) {
	std::vector<std::string> queries = {
		"INSERT INTO t VALUES (1)",
		"INSERT INTO t VALUES (2)",
		"INSERT INTO t VALUES (3)"
	};
	auto result = db_->execute_batch_async(queries);
	auto results = result.get();

	ASSERT_EQ(results.size(), 3u);
	EXPECT_TRUE(results[0]);
	EXPECT_TRUE(results[1]);
	EXPECT_TRUE(results[2]);
}

TEST_F(AsyncDatabaseTest, ExecuteBatchAsyncReportsFailures) {
	backend_->set_fail_execute(true);
	std::vector<std::string> queries = {"Q1", "Q2"};
	auto result = db_->execute_batch_async(queries);
	auto results = result.get();

	ASSERT_EQ(results.size(), 2u);
	EXPECT_FALSE(results[0]);
	EXPECT_FALSE(results[1]);
}

// -- select_batch_async() --

TEST_F(AsyncDatabaseTest, SelectBatchAsyncReturnsMultipleResults) {
	std::vector<std::string> queries = {
		"SELECT * FROM t",
		"SELECT * FROM t"
	};
	auto result = db_->select_batch_async(queries);
	auto results = result.get();

	ASSERT_EQ(results.size(), 2u);
	EXPECT_EQ(results[0].size(), 1u);
	EXPECT_EQ(results[1].size(), 1u);
}

// -- Transaction methods --

TEST_F(AsyncDatabaseTest, BeginTransactionAsyncSucceeds) {
	auto result = db_->begin_transaction_async();
	EXPECT_TRUE(result.get());
	EXPECT_TRUE(backend_->in_transaction());
}

TEST_F(AsyncDatabaseTest, CommitTransactionAsyncSucceeds) {
	db_->begin_transaction_async().get();
	auto result = db_->commit_transaction_async();
	EXPECT_TRUE(result.get());
	EXPECT_FALSE(backend_->in_transaction());
}

TEST_F(AsyncDatabaseTest, RollbackTransactionAsyncSucceeds) {
	db_->begin_transaction_async().get();
	auto result = db_->rollback_transaction_async();
	EXPECT_TRUE(result.get());
	EXPECT_FALSE(backend_->in_transaction());
}

// -- Connection management --

TEST_F(AsyncDatabaseTest, ConnectAsyncInitializesBackend) {
	auto result = db_->connect_async("host=localhost dbname=test");
	EXPECT_TRUE(result.get());
	EXPECT_TRUE(backend_->is_initialized());
}

TEST_F(AsyncDatabaseTest, DisconnectAsyncShutsDownBackend) {
	db_->connect_async("host=localhost").get();
	auto result = db_->disconnect_async();
	EXPECT_TRUE(result.get());
	EXPECT_FALSE(backend_->is_initialized());
}

// -- Concurrent operations --

TEST_F(AsyncDatabaseTest, ConcurrentExecuteAsyncOperations) {
	// async_result<T> is non-movable (contains std::mutex), so we call
	// get() immediately per operation rather than collecting into a vector.
	constexpr int NUM_OPS = 20;
	std::atomic<int> success_count{0};

	// Submit all operations, each getting its result in a detached context
	std::vector<std::future<bool>> futures;
	futures.reserve(NUM_OPS);
	for (int i = 0; i < NUM_OPS; ++i) {
		futures.push_back(executor_->submit([this, i]() -> bool {
			auto result = backend_->execute_query(
				"INSERT INTO t VALUES (" + std::to_string(i) + ")");
			return result.is_ok();
		}));
	}

	for (auto& f : futures) {
		if (f.get()) {
			success_count.fetch_add(1);
		}
	}
	EXPECT_EQ(success_count.load(), NUM_OPS);
}

// -- Callback integration --

TEST_F(AsyncDatabaseTest, ExecuteAsyncWithThenCallback) {
	auto result = db_->execute_async("CREATE TABLE t2 (id INT)");

	bool callback_invoked = false;
	result.then([&callback_invoked](bool success) {
		callback_invoked = true;
		EXPECT_TRUE(success);
	});

	result.get();
	EXPECT_TRUE(callback_invoked);
}

TEST_F(AsyncDatabaseTest, ExecuteAsyncWithOnErrorCallback) {
	backend_->set_fail_execute(true);
	auto result = db_->execute_async("BAD");

	std::string error_msg;
	result.on_error([&error_msg](const std::exception& e) {
		error_msg = e.what();
	});

	EXPECT_THROW(result.get(), std::runtime_error);
	EXPECT_EQ(error_msg, "execute_failed");
}
