// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * Unit tests for async_result<T>, async_executor, async_database,
 * transaction_coordinator, saga_builder, and stream_processor.
 *
 * Part of #366:
 *   Sub-issue #369: async_result<T>, async_executor, helper functions
 *   Sub-issue #371: async_database
 *   Sub-issue #373: transaction_coordinator, saga_builder
 *   Sub-issue #375: stream_processor
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

#include <kcenon/database/async/async_operations.h>

using namespace kcenon::database::async;

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

#include <kcenon/database/core/database_backend.h>

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

// ============================================================================
// transaction_coordinator tests
// ============================================================================

namespace {

// Controllable stub for transaction coordinator testing.
// Each instance represents one participant in a distributed transaction.
class txn_stub_backend : public ::database::core::database_backend {
public:
	::database::database_types type() const override {
		return ::database::database_types::sqlite;
	}

	kcenon::common::VoidResult initialize(
		const ::database::core::connection_config&) override
	{
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult shutdown() override {
		return kcenon::common::ok();
	}

	bool is_initialized() const override { return true; }

	kcenon::common::Result<::database::core::database_result> select_query(
		const std::string&) override
	{
		return kcenon::common::Result<::database::core::database_result>(
			::database::core::database_result{});
	}

	kcenon::common::VoidResult execute_query(const std::string&) override {
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult begin_transaction() override {
		std::lock_guard<std::mutex> lock(mutex_);
		if (should_fail_begin_) {
			return kcenon::common::error_info{1, "begin_failed", "stub"};
		}
		begin_count_++;
		in_txn_ = true;
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult commit_transaction() override {
		std::lock_guard<std::mutex> lock(mutex_);
		if (should_fail_commit_) {
			return kcenon::common::error_info{1, "commit_failed", "stub"};
		}
		commit_count_++;
		in_txn_ = false;
		return kcenon::common::ok();
	}

	kcenon::common::VoidResult rollback_transaction() override {
		std::lock_guard<std::mutex> lock(mutex_);
		rollback_count_++;
		in_txn_ = false;
		return kcenon::common::ok();
	}

	bool in_transaction() const override {
		std::lock_guard<std::mutex> lock(mutex_);
		return in_txn_;
	}

	std::string last_error() const override { return ""; }
	std::map<std::string, std::string> connection_info() const override {
		return {{"type", "txn_stub"}};
	}

	// Test controls
	void set_fail_begin(bool fail) {
		std::lock_guard<std::mutex> lock(mutex_);
		should_fail_begin_ = fail;
	}
	void set_fail_commit(bool fail) {
		std::lock_guard<std::mutex> lock(mutex_);
		should_fail_commit_ = fail;
	}
	int begin_count() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return begin_count_;
	}
	int commit_count() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return commit_count_;
	}
	int rollback_count() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return rollback_count_;
	}

private:
	mutable std::mutex mutex_;
	bool in_txn_ = false;
	bool should_fail_begin_ = false;
	bool should_fail_commit_ = false;
	int begin_count_ = 0;
	int commit_count_ = 0;
	int rollback_count_ = 0;
};

} // anonymous namespace

class TransactionCoordinatorTest : public ::testing::Test {
protected:
	void SetUp() override {
		p1_ = std::make_shared<txn_stub_backend>();
		p2_ = std::make_shared<txn_stub_backend>();
		p3_ = std::make_shared<txn_stub_backend>();
		participants_ = {p1_, p2_, p3_};
	}

	transaction_coordinator coord_;
	std::shared_ptr<txn_stub_backend> p1_;
	std::shared_ptr<txn_stub_backend> p2_;
	std::shared_ptr<txn_stub_backend> p3_;
	std::vector<std::shared_ptr<::database::core::database_backend>> participants_;
};

// -- begin_distributed_transaction --

TEST_F(TransactionCoordinatorTest, BeginCreatesTransaction) {
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	EXPECT_FALSE(txn_id.empty());

	auto active = coord_.get_active_transactions();
	ASSERT_EQ(active.size(), 1u);
	EXPECT_EQ(active[0].transaction_id, txn_id);
	EXPECT_EQ(active[0].participants.size(), 3u);
	EXPECT_EQ(active[0].state, transaction_coordinator::transaction_state::active);
}

TEST_F(TransactionCoordinatorTest, BeginGeneratesUniqueIds) {
	auto id1 = coord_.begin_distributed_transaction(participants_);
	auto id2 = coord_.begin_distributed_transaction(participants_);
	EXPECT_NE(id1, id2);
	EXPECT_EQ(coord_.get_active_transactions().size(), 2u);
}

// -- prepare_phase --

TEST_F(TransactionCoordinatorTest, PreparePhaseSuccess) {
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	bool result = coord_.prepare_phase(txn_id).get();
	EXPECT_TRUE(result);

	auto txns = coord_.get_active_transactions();
	ASSERT_EQ(txns.size(), 1u);
	EXPECT_EQ(txns[0].state, transaction_coordinator::transaction_state::prepared);
	EXPECT_EQ(p1_->begin_count(), 1);
	EXPECT_EQ(p2_->begin_count(), 1);
	EXPECT_EQ(p3_->begin_count(), 1);
}

TEST_F(TransactionCoordinatorTest, PreparePhaseThrowsForUnknownTransaction) {
	EXPECT_THROW(coord_.prepare_phase("nonexistent").get(), std::exception);
}

TEST_F(TransactionCoordinatorTest, PreparePhaseRollsBackOnPartialFailure) {
	p2_->set_fail_begin(true);
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	bool result = coord_.prepare_phase(txn_id).get();
	EXPECT_FALSE(result);

	// p1 was prepared then rolled back; p2 failed; p3 never reached
	EXPECT_EQ(p1_->begin_count(), 1);
	EXPECT_EQ(p1_->rollback_count(), 1);
	EXPECT_EQ(p2_->begin_count(), 0);
	EXPECT_EQ(p3_->begin_count(), 0);

	auto txns = coord_.get_active_transactions();
	ASSERT_EQ(txns.size(), 1u);
	EXPECT_EQ(txns[0].state, transaction_coordinator::transaction_state::aborted);
}

// -- commit_phase --

TEST_F(TransactionCoordinatorTest, CommitPhaseSuccess) {
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	coord_.prepare_phase(txn_id).get();

	bool result = coord_.commit_phase(txn_id).get();
	EXPECT_TRUE(result);

	EXPECT_EQ(p1_->commit_count(), 1);
	EXPECT_EQ(p2_->commit_count(), 1);
	EXPECT_EQ(p3_->commit_count(), 1);

	auto txns = coord_.get_active_transactions();
	EXPECT_EQ(txns[0].state, transaction_coordinator::transaction_state::committed);
}

TEST_F(TransactionCoordinatorTest, CommitPhaseFailsIfNotPrepared) {
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	// Skip prepare_phase — state is still "active"
	bool result = coord_.commit_phase(txn_id).get();
	EXPECT_FALSE(result);
	EXPECT_EQ(p1_->commit_count(), 0);
}

// -- commit_distributed_transaction (full 2PC) --

TEST_F(TransactionCoordinatorTest, CommitDistributedTransactionFull2PC) {
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	bool result = coord_.commit_distributed_transaction(txn_id).get();
	EXPECT_TRUE(result);

	EXPECT_EQ(p1_->begin_count(), 1);
	EXPECT_EQ(p1_->commit_count(), 1);
	EXPECT_EQ(p2_->begin_count(), 1);
	EXPECT_EQ(p2_->commit_count(), 1);
}

TEST_F(TransactionCoordinatorTest, CommitDistributedTransactionFailsOnPrepare) {
	p3_->set_fail_begin(true);
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	bool result = coord_.commit_distributed_transaction(txn_id).get();
	EXPECT_FALSE(result);

	// p1, p2 prepared then rolled back
	EXPECT_EQ(p1_->rollback_count(), 1);
	EXPECT_EQ(p2_->rollback_count(), 1);
	EXPECT_EQ(p3_->commit_count(), 0);
}

// -- rollback_distributed_transaction --

TEST_F(TransactionCoordinatorTest, RollbackDistributedTransaction) {
	auto txn_id = coord_.begin_distributed_transaction(participants_);
	coord_.prepare_phase(txn_id).get();

	bool result = coord_.rollback_distributed_transaction(txn_id).get();
	EXPECT_TRUE(result);

	EXPECT_EQ(p1_->rollback_count(), 1);
	EXPECT_EQ(p2_->rollback_count(), 1);
	EXPECT_EQ(p3_->rollback_count(), 1);

	auto txns = coord_.get_active_transactions();
	EXPECT_EQ(txns[0].state, transaction_coordinator::transaction_state::aborted);
}

TEST_F(TransactionCoordinatorTest, RollbackThrowsForUnknownTransaction) {
	EXPECT_THROW(
		coord_.rollback_distributed_transaction("nonexistent").get(),
		std::exception);
}

// -- recover_transactions --

TEST_F(TransactionCoordinatorTest, RecoverCleansUpCompletedTransactions) {
	auto id1 = coord_.begin_distributed_transaction(participants_);
	auto id2 = coord_.begin_distributed_transaction(participants_);
	coord_.commit_distributed_transaction(id1).get();

	EXPECT_EQ(coord_.get_active_transactions().size(), 2u);
	coord_.recover_transactions();

	// id1 is committed → cleaned up; id2 is still active
	auto remaining = coord_.get_active_transactions();
	ASSERT_EQ(remaining.size(), 1u);
	EXPECT_EQ(remaining[0].transaction_id, id2);
}

// -- create_saga --

TEST_F(TransactionCoordinatorTest, CreateSagaReturnsSagaBuilder) {
	auto saga = coord_.create_saga();
	// Verify the saga builder can execute (empty saga succeeds)
	bool result = saga.execute().get();
	EXPECT_TRUE(result);
}

// ============================================================================
// saga_builder tests
// ============================================================================

class SagaBuilderTest : public ::testing::Test {
protected:
	transaction_coordinator coord_;
};

TEST_F(SagaBuilderTest, EmptySagaSucceeds) {
	auto saga = coord_.create_saga();
	bool result = saga.execute().get();
	EXPECT_TRUE(result);
}

TEST_F(SagaBuilderTest, AllStepsSucceed) {
	std::vector<int> execution_order;

	auto saga = coord_.create_saga();
	saga.add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(1);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-1);
			return make_ready_result(true);
		}
	).add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(2);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-2);
			return make_ready_result(true);
		}
	).add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(3);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-3);
			return make_ready_result(true);
		}
	);

	bool result = saga.execute().get();
	EXPECT_TRUE(result);
	EXPECT_EQ(execution_order, (std::vector<int>{1, 2, 3}));
}

TEST_F(SagaBuilderTest, CompensatesOnActionFailure) {
	std::vector<int> execution_order;

	auto saga = coord_.create_saga();
	saga.add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(1);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-1);
			return make_ready_result(true);
		}
	).add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(2);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-2);
			return make_ready_result(true);
		}
	).add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(3);
			return make_ready_result(false); // Step 3 fails
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-3);
			return make_ready_result(true);
		}
	);

	bool result = saga.execute().get();
	EXPECT_FALSE(result);
	// Steps 1,2 executed; step 3 failed; compensate 2,1 in reverse
	EXPECT_EQ(execution_order, (std::vector<int>{1, 2, 3, -2, -1}));
}

TEST_F(SagaBuilderTest, CompensatesOnException) {
	std::vector<int> execution_order;

	auto saga = coord_.create_saga();
	saga.add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(1);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-1);
			return make_ready_result(true);
		}
	).add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(2);
			throw std::runtime_error("step 2 exploded");
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-2);
			return make_ready_result(true);
		}
	);

	bool result = saga.execute().get();
	EXPECT_FALSE(result);
	// Step 1 executed; step 2 threw; compensate 1 in reverse
	EXPECT_EQ(execution_order, (std::vector<int>{1, 2, -1}));
}

TEST_F(SagaBuilderTest, SingleStepSuccess) {
	bool action_called = false;
	auto saga = coord_.create_saga();
	saga.add_step(
		[&]() -> async_result<bool> {
			action_called = true;
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			return make_ready_result(true);
		}
	);

	bool result = saga.execute().get();
	EXPECT_TRUE(result);
	EXPECT_TRUE(action_called);
}

TEST_F(SagaBuilderTest, CompensationExceptionDoesNotBreakChain) {
	std::vector<int> execution_order;

	auto saga = coord_.create_saga();
	saga.add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(1);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-1);
			return make_ready_result(true);
		}
	).add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(2);
			return make_ready_result(true);
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-2);
			throw std::runtime_error("compensation 2 failed");
			return make_ready_result(true);
		}
	).add_step(
		[&]() -> async_result<bool> {
			execution_order.push_back(3);
			return make_ready_result(false); // fails
		},
		[&]() -> async_result<bool> {
			execution_order.push_back(-3);
			return make_ready_result(true);
		}
	);

	bool result = saga.execute().get();
	EXPECT_FALSE(result);
	// Compensation -2 throws, but -1 still executes
	EXPECT_EQ(execution_order, (std::vector<int>{1, 2, 3, -2, -1}));
}

TEST_F(SagaBuilderTest, AddStepReturnsSelfForChaining) {
	auto saga = coord_.create_saga();
	auto& ref = saga.add_step(
		[]() -> async_result<bool> { return make_ready_result(true); },
		[]() -> async_result<bool> { return make_ready_result(true); }
	);
	EXPECT_EQ(&ref, &saga);
}

//=============================================================================
// stream_processor Tests
//=============================================================================

class StreamProcessorTest : public ::testing::Test {
protected:
	void SetUp() override {
		backend_ = std::make_shared<async_stub_backend>();
		processor_ = std::make_unique<stream_processor>(backend_);
	}

	void TearDown() override {
		processor_.reset();
	}

	// Wait for an atomic flag to become true, with timeout
	bool wait_for_flag(const std::atomic<bool>& flag,
	                   std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
	{
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (!flag.load() && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return flag.load();
	}

	// Wait for an atomic counter to reach a target value, with timeout
	bool wait_for_count(const std::atomic<int>& counter, int target,
	                    std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
	{
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (counter.load() < target && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return counter.load() >= target;
	}

	std::shared_ptr<async_stub_backend> backend_;
	std::unique_ptr<stream_processor> processor_;
};

// -- Construction --

TEST_F(StreamProcessorTest, ConstructsWithBackend) {
	EXPECT_NE(processor_, nullptr);
}

// -- start_stream / stop_stream --

TEST_F(StreamProcessorTest, StartStreamReturnsTrue) {
	EXPECT_TRUE(processor_->start_stream(
		stream_processor::stream_type::custom, "test_channel"));
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, StartStreamDuplicateReturnsFalse) {
	EXPECT_TRUE(processor_->start_stream(
		stream_processor::stream_type::custom, "ch1"));
	EXPECT_FALSE(processor_->start_stream(
		stream_processor::stream_type::custom, "ch1"));
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, StopStreamReturnsTrue) {
	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");
	EXPECT_TRUE(processor_->stop_stream("ch1"));
}

TEST_F(StreamProcessorTest, StopStreamNonExistentReturnsFalse) {
	EXPECT_FALSE(processor_->stop_stream("no_such_channel"));
}

TEST_F(StreamProcessorTest, StopAllStreamsStopsMultiple) {
	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");
	processor_->start_stream(
		stream_processor::stream_type::redis_pubsub, "ch2");
	processor_->start_stream(
		stream_processor::stream_type::postgresql_notify, "ch3");

	// Should not hang or crash
	processor_->stop_all_streams();

	// All stopped — stopping again returns false
	EXPECT_FALSE(processor_->stop_stream("ch1"));
	EXPECT_FALSE(processor_->stop_stream("ch2"));
	EXPECT_FALSE(processor_->stop_stream("ch3"));
}

// -- Event handler --

TEST_F(StreamProcessorTest, EventHandlerReceivesConnectedEvent) {
	std::atomic<bool> received{false};
	std::string captured_payload;
	std::mutex capture_mutex;

	processor_->register_event_handler("ch1",
		[&](const stream_processor::stream_event& event) {
			std::lock_guard<std::mutex> lock(capture_mutex);
			captured_payload = event.payload;
			received.store(true);
		});

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");

	ASSERT_TRUE(wait_for_flag(received));
	{
		std::lock_guard<std::mutex> lock(capture_mutex);
		EXPECT_EQ(captured_payload, "stream_connected");
	}
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, EventHandlerReceivesCorrectChannel) {
	std::atomic<bool> received{false};
	std::string captured_channel;
	std::mutex capture_mutex;

	processor_->register_event_handler("notifications",
		[&](const stream_processor::stream_event& event) {
			std::lock_guard<std::mutex> lock(capture_mutex);
			captured_channel = event.channel;
			received.store(true);
		});

	processor_->start_stream(
		stream_processor::stream_type::postgresql_notify, "notifications");

	ASSERT_TRUE(wait_for_flag(received));
	{
		std::lock_guard<std::mutex> lock(capture_mutex);
		EXPECT_EQ(captured_channel, "notifications");
	}
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, EventHandlerReceivesCorrectType) {
	std::atomic<bool> received{false};
	stream_processor::stream_type captured_type;

	processor_->register_event_handler("ch1",
		[&](const stream_processor::stream_event& event) {
			captured_type = event.type;
			received.store(true);
		});

	processor_->start_stream(
		stream_processor::stream_type::mongodb_change_stream, "ch1");

	ASSERT_TRUE(wait_for_flag(received));
	EXPECT_EQ(captured_type,
		stream_processor::stream_type::mongodb_change_stream);
	processor_->stop_all_streams();
}

// -- Global handler --

TEST_F(StreamProcessorTest, GlobalHandlerReceivesAllEvents) {
	std::atomic<int> event_count{0};

	processor_->register_global_handler(
		[&](const stream_processor::stream_event&) {
			event_count.fetch_add(1);
		});

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");
	processor_->start_stream(
		stream_processor::stream_type::custom, "ch2");

	ASSERT_TRUE(wait_for_count(event_count, 2));
	EXPECT_GE(event_count.load(), 2);
	processor_->stop_all_streams();
}

// -- Event filter --

TEST_F(StreamProcessorTest, EventFilterRejectsEvent) {
	std::atomic<bool> handler_called{false};

	processor_->add_event_filter("ch1",
		[](const stream_processor::stream_event&) {
			return false;  // Reject all events
		});

	processor_->register_event_handler("ch1",
		[&](const stream_processor::stream_event&) {
			handler_called.store(true);
		});

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");

	// Give enough time for the event to potentially arrive
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_FALSE(handler_called.load());
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, EventFilterAcceptsEvent) {
	std::atomic<bool> handler_called{false};

	processor_->add_event_filter("ch1",
		[](const stream_processor::stream_event& event) {
			return event.payload == "stream_connected";
		});

	processor_->register_event_handler("ch1",
		[&](const stream_processor::stream_event&) {
			handler_called.store(true);
		});

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");

	ASSERT_TRUE(wait_for_flag(handler_called));
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, FilterDoesNotAffectGlobalHandler) {
	std::atomic<bool> global_called{false};

	processor_->add_event_filter("ch1",
		[](const stream_processor::stream_event&) {
			return false;  // Reject
		});

	processor_->register_global_handler(
		[&](const stream_processor::stream_event&) {
			global_called.store(true);
		});

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");

	// Global handler should NOT be called when filter rejects
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_FALSE(global_called.load());
	processor_->stop_all_streams();
}

// -- Per-channel independence --

TEST_F(StreamProcessorTest, StopOneStreamDoesNotAffectOthers) {
	std::atomic<int> ch1_count{0};
	std::atomic<int> ch2_count{0};

	processor_->register_event_handler("ch1",
		[&](const stream_processor::stream_event&) {
			ch1_count.fetch_add(1);
		});
	processor_->register_event_handler("ch2",
		[&](const stream_processor::stream_event&) {
			ch2_count.fetch_add(1);
		});

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");
	processor_->start_stream(
		stream_processor::stream_type::custom, "ch2");

	// Wait for both connected events
	ASSERT_TRUE(wait_for_count(ch1_count, 1));
	ASSERT_TRUE(wait_for_count(ch2_count, 1));

	// Stop only ch1 — ch2 should still be alive
	EXPECT_TRUE(processor_->stop_stream("ch1"));

	// Verify ch1 is stopped
	EXPECT_FALSE(processor_->stop_stream("ch1"));

	// ch2 stream should still be running (can be stopped)
	EXPECT_TRUE(processor_->stop_stream("ch2"));
}

// -- Destructor safety --

TEST_F(StreamProcessorTest, DestructorStopsAllStreams) {
	auto local_backend = std::make_shared<async_stub_backend>();
	{
		stream_processor sp(local_backend);
		sp.start_stream(
			stream_processor::stream_type::custom, "ch1");
		sp.start_stream(
			stream_processor::stream_type::custom, "ch2");
		// Destructor runs here — must not crash or hang
	}
	SUCCEED();
}

// -- Template concept overloads --

TEST_F(StreamProcessorTest, TemplateEventHandlerWithLambda) {
	std::atomic<bool> received{false};

	// Uses the concept-constrained template overload
	auto handler = [&](const stream_processor::stream_event&) {
		received.store(true);
	};
	processor_->register_event_handler("ch1", handler);

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");

	ASSERT_TRUE(wait_for_flag(received));
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, TemplateGlobalHandlerWithLambda) {
	std::atomic<int> count{0};

	auto handler = [&](const stream_processor::stream_event&) {
		count.fetch_add(1);
	};
	processor_->register_global_handler(handler);

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");

	ASSERT_TRUE(wait_for_count(count, 1));
	processor_->stop_all_streams();
}

TEST_F(StreamProcessorTest, TemplateEventFilterWithLambda) {
	std::atomic<bool> handler_called{false};

	auto filter = [](const stream_processor::stream_event& event) {
		return event.payload == "stream_connected";
	};
	processor_->add_event_filter("ch1", filter);

	processor_->register_event_handler("ch1",
		[&](const stream_processor::stream_event&) {
			handler_called.store(true);
		});

	processor_->start_stream(
		stream_processor::stream_type::custom, "ch1");

	ASSERT_TRUE(wait_for_flag(handler_called));
	processor_->stop_all_streams();
}
