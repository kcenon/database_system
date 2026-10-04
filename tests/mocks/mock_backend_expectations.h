// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/core/database_backend.h>
#include <kcenon/common/patterns/result.h>
#include <string>
#include <functional>
#include <regex>
#include <optional>
#include <stdexcept>

namespace kcenon::database::testing {

// Forward declaration
class mock_backend;

/**
 * @enum backend_match_type
 * @brief How to match query strings
 */
enum class backend_match_type {
    EXACT,   ///< Exact string match
    PATTERN, ///< Regex pattern match
    ANY      ///< Match any query
};

/**
 * @class backend_expectation
 * @brief Single query expectation with configurable behavior for database_backend
 *
 * Unlike the legacy expectation class, this one works with Result<T> types
 * for proper error handling.
 */
class backend_expectation {
public:
    backend_expectation();

    // Query matching
    backend_expectation& for_query(const std::string& query, backend_match_type type = backend_match_type::EXACT);
    backend_expectation& for_pattern(const std::string& pattern);
    backend_expectation& for_any();

    // Response configuration with Result types
    backend_expectation& returning(const core::database_result& result);
    backend_expectation& returning_rows_affected(uint64_t count);
    backend_expectation& returning_error(const std::string& error_message);
    backend_expectation& returning_execute_success();

    // Invocation limits
    backend_expectation& times(int count);
    backend_expectation& at_least(int count);
    backend_expectation& at_most(int count);
    backend_expectation& once();
    backend_expectation& never();

    // Matching and invocation
    bool matches(const std::string& query) const;
    bool is_satisfied() const;
    bool can_be_invoked() const;

    // Get results as Result<T> types
    kcenon::common::Result<core::database_result> get_select_result();
    kcenon::common::Result<uint64_t> get_rows_affected();
    kcenon::common::VoidResult get_execute_result();

    // Check if should return error
    bool should_error() const;
    std::string get_error_message() const;

private:
    std::string query_;
    backend_match_type match_type_;
    std::regex pattern_;

    std::optional<core::database_result> result_;
    std::optional<uint64_t> rows_affected_;
    std::optional<std::string> error_message_;
    bool execute_success_;

    int min_invocations_;
    int max_invocations_;
    int actual_invocations_;
};

/**
 * @class backend_expectation_builder
 * @brief Fluent builder for backend expectations
 */
class backend_expectation_builder {
public:
    backend_expectation_builder(mock_backend* db, backend_expectation exp);

    // Response configuration
    backend_expectation_builder& will_return(const core::database_result& result);
    backend_expectation_builder& will_return_rows(uint64_t count);
    backend_expectation_builder& will_fail(const std::string& error_message);
    backend_expectation_builder& will_succeed();

    // Invocation limits
    backend_expectation_builder& times(int count);
    backend_expectation_builder& once();
    backend_expectation_builder& any_times();

private:
    mock_backend* db_;
    backend_expectation exp_;
    bool pushed_ = false;
};

/**
 * @class backend_exception
 * @brief Exception thrown by mock when simulating errors
 */
class backend_exception : public std::runtime_error {
public:
    explicit backend_exception(const std::string& message)
        : std::runtime_error(message) {}
};

} // namespace kcenon::database::testing
