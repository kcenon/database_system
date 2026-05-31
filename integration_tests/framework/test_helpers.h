// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/database_types.h>
#include <kcenon/database/core/database_backend.h>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace kcenon::database::testing {
/**
 * @class PerformanceTimer
 * @brief Simple timer for measuring performance.
 */
class PerformanceTimer {
public:
  PerformanceTimer() : start_(std::chrono::high_resolution_clock::now()) {}

  void Reset() { start_ = std::chrono::high_resolution_clock::now(); }

  template <typename Duration = std::chrono::milliseconds>
  int64_t Elapsed() const {
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<Duration>(end - start_).count();
  }

  double ElapsedSeconds() const {
    return Elapsed<std::chrono::microseconds>() / 1'000'000.0;
  }

private:
  std::chrono::high_resolution_clock::time_point start_;
};

/**
 * @class LatencyTracker
 * @brief Tracks latency measurements for performance testing.
 */
class LatencyTracker {
public:
  void Record(int64_t latency_us) { latencies_.push_back(latency_us); }

  size_t Count() const { return latencies_.size(); }

  double Mean() const {
    if (latencies_.empty()) {
      return 0.0;
    }
    double sum = std::accumulate(latencies_.begin(), latencies_.end(), 0.0);
    return sum / latencies_.size();
  }

  double Percentile(double p) const {
    if (latencies_.empty()) {
      return 0.0;
    }

    std::vector<int64_t> sorted = latencies_;
    std::sort(sorted.begin(), sorted.end());

    size_t index = static_cast<size_t>(p * sorted.size());
    if (index >= sorted.size()) {
      index = sorted.size() - 1;
    }

    return static_cast<double>(sorted[index]);
  }

  double P50() const { return Percentile(0.50); }
  double P95() const { return Percentile(0.95); }
  double P99() const { return Percentile(0.99); }

  double Min() const {
    if (latencies_.empty()) {
      return 0.0;
    }
    return static_cast<double>(
        *std::min_element(latencies_.begin(), latencies_.end()));
  }

  double Max() const {
    if (latencies_.empty()) {
      return 0.0;
    }
    return static_cast<double>(
        *std::max_element(latencies_.begin(), latencies_.end()));
  }

  void Clear() { latencies_.clear(); }

private:
  std::vector<int64_t> latencies_;
};

/**
 * @brief Generates random string of specified length.
 */
inline std::string GenerateRandomString(size_t length) {
  static const char alphanum[] = "0123456789"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

  std::string result;
  result.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    result += alphanum[dis(gen)];
  }
  return result;
}

/**
 * @brief Generates random integer in range [min, max].
 */
inline int GenerateRandomInt(int min, int max) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(min, max);
  return dis(gen);
}

/**
 * @brief Verifies data in query result.
 */
inline bool VerifyData(const core::database_result &result, const std::string &column,
                       const std::string &expected_value) {
  if (result.empty()) {
    return false;
  }

  auto it = result[0].find(column);
  if (it == result[0].end()) {
    return false;
  }

  // Extract string value from variant and compare
  try {
    if (std::holds_alternative<std::string>(it->second)) {
      return std::get<std::string>(it->second) == expected_value;
    }
    // Try converting other types to string for comparison
    if (std::holds_alternative<int64_t>(it->second)) {
      return std::to_string(std::get<int64_t>(it->second)) == expected_value;
    }
    if (std::holds_alternative<double>(it->second)) {
      return std::to_string(std::get<double>(it->second)) == expected_value;
    }
    if (std::holds_alternative<bool>(it->second)) {
      return (std::get<bool>(it->second) ? "true" : "false") == expected_value;
    }
  } catch (...) {
    return false;
  }
  return false;
}

/**
 * @brief Checks if row count matches expected value.
 */
inline bool CheckRowCount(const core::database_result &result, size_t expected) {
  return result.size() == expected;
}

/**
 * @brief Validates connection string format for SQLite.
 */
inline bool ValidateConnectionString(const std::string &conn_str) {
  // Basic validation for SQLite connection strings
  if (conn_str.empty()) {
    return false;
  }

  // Reject protocol prefixes (those are for other databases)
  if (conn_str.find("://") != std::string::npos) {
    return false;
  }

  // Accept: .db files, paths with /, or :memory:
  return conn_str.find(".db") != std::string::npos ||
         conn_str.find("/") != std::string::npos || conn_str == ":memory:";
}

/**
 * @brief Creates an in-memory SQLite connection string.
 */
inline std::string CreateMemoryConnectionString() { return ":memory:"; }

/**
 * @brief Creates a file-based SQLite connection string.
 */
inline std::string CreateFileConnectionString(const std::string &filename) {
  return filename; // SQLite accepts absolute paths directly
}

/**
 * @class TransactionHelper
 * @brief Helper for transaction management in tests.
 */
class TransactionHelper {
public:
  explicit TransactionHelper(database_manager *manager)
      : manager_(manager), active_(false) {}

  bool Begin() {
    if (active_) {
      return false;
    }
    active_ = manager_->create_query_result("BEGIN TRANSACTION").is_ok();
    return active_;
  }

  bool Commit() {
    if (!active_) {
      return false;
    }
    bool result = manager_->create_query_result("COMMIT").is_ok();
    active_ = false;
    return result;
  }

  bool Rollback() {
    if (!active_) {
      return false;
    }
    bool result = manager_->create_query_result("ROLLBACK").is_ok();
    active_ = false;
    return result;
  }

  bool IsActive() const { return active_; }

private:
  database_manager *manager_;
  bool active_;
};

/**
 * @brief Waits for a condition with timeout.
 */
template <typename Predicate>
bool WaitFor(Predicate pred, std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  while (!pred()) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed >= timeout) {
      return false;
    }
    std::this_thread::yield();
  }
  return true;
}

/**
 * @brief Measures throughput for a callable operation.
 */
template <typename Func>
double MeasureThroughput(Func &&func, std::chrono::milliseconds duration) {
  size_t operations = 0;
  auto start = std::chrono::high_resolution_clock::now();
  auto end = start + duration;

  while (std::chrono::high_resolution_clock::now() < end) {
    func();
    ++operations;
  }

  auto elapsed = std::chrono::high_resolution_clock::now() - start;
  double seconds = std::chrono::duration<double>(elapsed).count();
  return operations / seconds;
}

} // namespace kcenon::database::testing
