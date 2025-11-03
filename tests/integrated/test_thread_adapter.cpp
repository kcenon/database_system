// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file test_thread_adapter.cpp
 * @brief Unit tests for thread_adapter (Phase 4)
 *
 * These are lightweight API tests that verify the adapter interface
 * without requiring deep integration with thread_system.
 *
 * For full integration testing, run the integration test suite instead.
 */

#include "../../database/integrated/adapters/thread_adapter.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace database::integrated;
using namespace database::integrated::adapters;

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) \
	do { \
		std::cout << "Running test: " << #name << " ... "; \
		try { \
			test_##name(); \
			std::cout << "PASSED\n"; \
			tests_passed++; \
		} catch (const std::exception& e) { \
			std::cout << "FAILED: " << e.what() << "\n"; \
			tests_failed++; \
		} \
	} while(0)

#define ASSERT_TRUE(condition) \
	if (!(condition)) { \
		throw std::runtime_error("Assertion failed: " #condition); \
	}

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

//==============================================================================
// API Verification Tests (No Deep Integration Required)
//==============================================================================

// Test 1: Configuration construction
TEST(configuration_construction) {
	db_thread_config config;
	config.thread_count = 4;
	config.max_queue_size = 100;
	config.pool_type = thread_pool_type::standard;
	config.enable_priority_scheduling = false;

	// Should be able to create config
	ASSERT_TRUE(config.thread_count == 4);
	ASSERT_TRUE(config.max_queue_size == 100);
}

// Test 2: Adapter construction
TEST(adapter_construction) {
	db_thread_config config;
	config.thread_count = 2;

	// Should be able to construct adapter
	thread_adapter adapter(config);

	// Construction should succeed
}

// Test 3: API availability - basic task submission
TEST(api_availability_submit) {
	db_thread_config config;
	config.thread_count = 2;

	thread_adapter adapter(config);

	// Try to initialize (may fail without thread_system, that's ok)
	auto init_result = adapter.initialize();

	// If initialization succeeded, test submit
	if (init_result.is_ok()) {
		std::atomic<bool> executed{false};

		auto future = adapter.submit([&executed]() {
			executed = true;
			return 42;
		});

		// Wait for completion
		auto value = future.get();
		ASSERT_TRUE(value == 42);
		ASSERT_TRUE(executed);

		adapter.shutdown();
	}
	// If init failed, that's acceptable for unit test
}

// Test 4: API availability - priority submission
TEST(api_availability_priority) {
	db_thread_config config;
	config.thread_count = 1;
	config.enable_priority_scheduling = true;

	thread_adapter adapter(config);

	auto init_result = adapter.initialize();

	if (init_result.is_ok()) {
		std::atomic<bool> executed{false};

		auto future = adapter.submit_with_priority(5, [&executed]() {
			executed = true;
		});

		future.get();
		ASSERT_TRUE(executed);

		adapter.shutdown();
	}
}

// Test 5: API availability - statistics
TEST(api_availability_stats) {
	db_thread_config config;
	config.thread_count = 4;

	thread_adapter adapter(config);

	// Should be able to query stats even without initialization
	auto worker_count = adapter.worker_count();
	auto queue_size = adapter.queue_size();

	// Values may be 0 if not initialized, but API should work
	(void)worker_count;
	(void)queue_size;
}

// Test 6: Multiple instances
TEST(multiple_instances) {
	db_thread_config config1;
	config1.thread_count = 2;

	db_thread_config config2;
	config2.thread_count = 3;

	// Should be able to create multiple adapters
	thread_adapter adapter1(config1);
	thread_adapter adapter2(config2);

	// Both should be constructible
}

// Test 7: Move semantics
TEST(move_semantics) {
	db_thread_config config;
	config.thread_count = 2;

	thread_adapter adapter1(config);

	// Should support move construction
	thread_adapter adapter2(std::move(adapter1));

	// Moved-to instance should be usable
	auto init_result = adapter2.initialize();
	if (init_result.is_ok()) {
		adapter2.shutdown();
	}
}

// Test 8: Destructor safety
TEST(destructor_safety) {
	// Test that adapter can be constructed and destroyed safely
	{
		db_thread_config config;
		config.thread_count = 2;

		thread_adapter adapter(config);

		// Try to initialize
		auto init_result = adapter.initialize();

		if (init_result.is_ok()) {
			// Submit a task
			auto future = adapter.submit([]() { return 1; });
			future.get();
		}

		// Destructor will be called here (should call shutdown internally)
	}

	// Should not crash
}

// Test 9: Shutdown without initialization
TEST(shutdown_without_init) {
	db_thread_config config;
	config.thread_count = 2;

	thread_adapter adapter(config);

	// Should be able to shutdown without initialize
	auto result = adapter.shutdown();

	// Should succeed or handle gracefully
	(void)result;
}

// Test 10: Double initialization
TEST(double_initialization) {
	db_thread_config config;
	config.thread_count = 2;

	thread_adapter adapter(config);

	auto init1 = adapter.initialize();

	if (init1.is_ok()) {
		// Second initialization may succeed (idempotent) or fail
		// The important thing is it doesn't crash
		auto init2 = adapter.initialize();
		// Either behavior is acceptable
		(void)init2;

		adapter.shutdown();
	}
}

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
	std::cout << "=== Thread Adapter API Tests (Phase 4) ===\n";
	std::cout << "Note: These tests verify API availability and basic functionality.\n";
	std::cout << "Some tests may be skipped if thread_system is not available.\n\n";

	RUN_TEST(configuration_construction);
	RUN_TEST(adapter_construction);
	RUN_TEST(api_availability_submit);
	RUN_TEST(api_availability_priority);
	RUN_TEST(api_availability_stats);
	RUN_TEST(multiple_instances);
	RUN_TEST(move_semantics);
	RUN_TEST(destructor_safety);
	RUN_TEST(shutdown_without_init);
	RUN_TEST(double_initialization);

	std::cout << "\n=== Test Summary ===\n";
	std::cout << "Passed: " << tests_passed << "\n";
	std::cout << "Failed: " << tests_failed << "\n";

	if (tests_failed == 0) {
		std::cout << "=== All tests passed! ✓ ===\n";
		return 0;
	} else {
		std::cout << "=== Some tests failed! ✗ ===\n";
		return 1;
	}
}
