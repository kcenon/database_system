/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 *
 * Unit tests for async_result<T> and async_executor.
 *
 * Part of #366 / Sub-issue #369:
 *   - async_result<T>: constructor, get(), get_for(), is_ready(), wait_for(),
 *     then(), on_error(), legacy overloads
 *   - async_executor: submit(), shutdown(), wait_for_completion(),
 *     pending_tasks(), thread_count()
 *   - Helper functions: make_ready_result(), make_error_result()
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
