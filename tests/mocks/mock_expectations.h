/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, Database System
All rights reserved.
*****************************************************************************/

#pragma once

#include "database/database_base.h"
#include <string>
#include <functional>
#include <regex>
#include <optional>
#include <stdexcept>

namespace database::testing {

// Forward declaration
class mock_database;

/**
 * @enum match_type
 * @brief How to match query strings
 */
enum class match_type {
    EXACT,   ///< Exact string match
    PATTERN, ///< Regex pattern match
    ANY      ///< Match any query
};

/**
 * @class expectation
 * @brief Single query expectation with configurable behavior
 */
class expectation {
public:
    expectation();

    // Query matching
    expectation& for_query(const std::string& query, match_type type = match_type::EXACT);
    expectation& for_pattern(const std::string& pattern);
    expectation& for_any();

    // Response configuration
    expectation& returning(const database_result& result);
    expectation& returning_rows_affected(unsigned int count);
    expectation& throwing(const std::string& error_message);
    expectation& returning_execute_result(bool result);

    // Invocation limits
    expectation& times(int count);
    expectation& at_least(int count);
    expectation& at_most(int count);
    expectation& once();
    expectation& never();

    // Matching and invocation
    bool matches(const std::string& query) const;
    bool is_satisfied() const;
    bool can_be_invoked() const;

    // Get results
    database_result get_result();
    unsigned int get_rows_affected();
    bool get_execute_result();

    // Check if should throw
    bool should_throw() const;
    std::string get_error_message() const;

private:
    std::string query_;
    match_type match_type_;
    std::regex pattern_;

    std::optional<database_result> result_;
    std::optional<unsigned int> rows_affected_;
    std::optional<bool> execute_result_;
    std::optional<std::string> error_message_;

    int min_invocations_;
    int max_invocations_;
    int actual_invocations_;
};

/**
 * @class expectation_builder
 * @brief Fluent builder for expectations
 */
class expectation_builder {
public:
    expectation_builder(mock_database* db, expectation exp);

    // Response configuration
    expectation_builder& will_return(const database_result& result);
    expectation_builder& will_return_rows(unsigned int count);
    expectation_builder& will_fail(const std::string& error_message);
    expectation_builder& will_succeed();

    // Invocation limits
    expectation_builder& times(int count);
    expectation_builder& once();
    expectation_builder& any_times();

private:
    mock_database* db_;
    expectation exp_;
};

/**
 * @class database_exception
 * @brief Exception thrown by mock when simulating errors
 */
class database_exception : public std::runtime_error {
public:
    explicit database_exception(const std::string& message)
        : std::runtime_error(message) {}
};

} // namespace database::testing
