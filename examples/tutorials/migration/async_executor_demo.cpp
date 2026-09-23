// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file async_executor_demo.cpp
 * @brief High-performance async_executor with thread_system integration
 * @example async_executor_demo.cpp
 *
 * Demonstrates the async_executor for efficient task scheduling:
 * - Basic async task submission and result collection via std::future
 * - High-throughput performance testing (10,000+ tasks)
 * - Exception propagation through futures
 * - Graceful shutdown with pending task completion
 * - Performance comparison between async_executor and std::async
 *
 * @note When thread_system is available, the executor achieves ~77ns latency
 *       per task; otherwise it falls back to ~2-5us per task.
 */

#include <iostream>
#include <chrono>
#include <vector>
#include "database/async/async_operations.h"

using namespace database::async;
using namespace std::chrono;

void demonstrate_basic_usage() {
    std::cout << "=== Basic async_executor Usage ===\n";

    // Create executor with default thread count
    async_executor executor;

    std::cout << "Executor created with " << executor.thread_count() << " threads\n";
    std::cout << "Using thread_system: " << (executor.is_using_thread_system() ? "YES" : "NO") << "\n\n";

    // Submit simple tasks
    std::cout << "Submitting 5 simple tasks...\n";
    std::vector<std::future<int>> futures;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < 5; ++i) {
        auto future = executor.submit([i]() {
            // Simulate some work
            std::this_thread::sleep_for(milliseconds(100));
            return i * i;
        });
        futures.push_back(std::move(future));
    }

    // Collect results
    std::cout << "Collecting results:\n";
    for (size_t i = 0; i < futures.size(); ++i) {
        int result = futures[i].get();
        std::cout << "  Task " << i << " result: " << result << "\n";
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    std::cout << "Total time: " << duration.count() << "ms\n";
    std::cout << "(Expected ~100ms with concurrent execution)\n\n";
}

void demonstrate_high_throughput() {
    std::cout << "=== High-Throughput Performance Test ===\n";

    async_executor executor(8);

    const int num_tasks = 10000;
    std::cout << "Submitting " << num_tasks << " lightweight tasks...\n";

    auto start = high_resolution_clock::now();

    std::vector<std::future<int>> futures;
    futures.reserve(num_tasks);

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(executor.submit([i]() {
            // Very lightweight task
            return i * 2;
        }));
    }

    // Wait for completion
    executor.wait_for_completion();

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    std::cout << "Submitted " << num_tasks << " tasks in "
              << duration.count() << " microseconds\n";
    std::cout << "Average latency: "
              << (duration.count() / num_tasks) << " microseconds/task\n";

    if (executor.is_using_thread_system()) {
        std::cout << "✅ thread_system: Expected ~77ns latency per task\n";
    } else {
        std::cout << "⚠️  Fallback mode: Expected ~2-5μs latency per task\n";
    }

    std::cout << "\n";
}

void demonstrate_error_handling() {
    std::cout << "=== Error Handling ===\n";

    async_executor executor(4);

    // Submit task that throws exception
    auto future = executor.submit([]() -> int {
        throw std::runtime_error("Simulated error in task");
        return 42;
    });

    try {
        int result = future.get();
        std::cout << "Result: " << result << "\n";
    } catch (const std::exception& e) {
        std::cout << "✅ Caught exception: " << e.what() << "\n";
    }

    std::cout << "\n";
}

void demonstrate_shutdown() {
    std::cout << "=== Graceful Shutdown ===\n";

    async_executor executor(4);

    // Submit long-running tasks
    std::cout << "Submitting 3 long-running tasks...\n";
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 3; ++i) {
        futures.push_back(executor.submit([i]() {
            std::cout << "  Task " << i << " starting...\n";
            std::this_thread::sleep_for(milliseconds(200));
            std::cout << "  Task " << i << " completed\n";
        }));
    }

    std::cout << "Pending tasks: " << executor.pending_tasks() << "\n";

    // Wait for completion
    std::cout << "Waiting for tasks to complete...\n";
    for (auto& future : futures) {
        future.get();
    }

    std::cout << "✅ All tasks completed, shutting down...\n";
    executor.shutdown();
    std::cout << "✅ Executor shut down gracefully\n\n";
}

void compare_with_legacy() {
    std::cout << "=== Performance Comparison ===\n";
    std::cout << "async_executor vs std::async\n\n";

    const int num_tasks = 1000;

    // Test async_executor
    {
        async_executor executor(8);
        auto start = high_resolution_clock::now();

        std::vector<std::future<int>> futures;
        for (int i = 0; i < num_tasks; ++i) {
            futures.push_back(executor.submit([i]() { return i * 2; }));
        }

        for (auto& f : futures) {
            f.get();
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "async_executor: " << duration.count() << " μs\n";
        std::cout << "  (" << (duration.count() / num_tasks) << " μs/task)\n";
    }

    // Test std::async
    {
        auto start = high_resolution_clock::now();

        std::vector<std::future<int>> futures;
        for (int i = 0; i < num_tasks; ++i) {
            futures.push_back(std::async(std::launch::async, [i]() { return i * 2; }));
        }

        for (auto& f : futures) {
            f.get();
        }

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);

        std::cout << "std::async:        " << duration.count() << " μs\n";
        std::cout << "  (" << (duration.count() / num_tasks) << " μs/task)\n";
    }

    std::cout << "\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════════╗\n";
    std::cout << "║  async_executor Demonstration              ║\n";
    std::cout << "║  thread_system Integration for Database       ║\n";
    std::cout << "╚════════════════════════════════════════════════╝\n\n";

    try {
        demonstrate_basic_usage();
        demonstrate_high_throughput();
        demonstrate_error_handling();
        demonstrate_shutdown();
        compare_with_legacy();

        std::cout << "╔════════════════════════════════════════════════╗\n";
        std::cout << "║  ✅ All demonstrations completed successfully ║\n";
        std::cout << "╚════════════════════════════════════════════════╝\n";

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
}
