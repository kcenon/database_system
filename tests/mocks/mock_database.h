// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/core/database_backend.h>
#include "mock_expectations.h"
#include <queue>
#include <functional>
#include <mutex>
#include <regex>

namespace database::testing {

/**
 * @class mock_database
 * @brief Configurable mock for database_backend interface
 *
 * Features:
 * - Set expected queries and results
 * - Simulate errors and exceptions
 * - Record all executed queries for verification
 * - Pattern matching for flexible expectations
 *
 * Example usage:
 * @code
 *   mock_database db;
 *
 *   // Set up expectation
 *   db.expect_query("SELECT * FROM users")
 *     .will_return({{{"id", 1}, {"name", std::string("John")}}});
 *
 *   // Execute query
 *   auto result = db.select_query("SELECT * FROM users");
 *
 *   // Verify
 *   EXPECT_TRUE(db.verify_all_expectations());
 * @endcode
 */
class mock_database : public core::database_backend {
public:
    mock_database();
    ~mock_database() override = default;

    // Non-copyable due to mutex
    mock_database(const mock_database&) = delete;
    mock_database& operator=(const mock_database&) = delete;

    // Movable
    mock_database(mock_database&& other) noexcept;
    mock_database& operator=(mock_database&& other) noexcept;

    // database_backend interface implementation
    database_types type() const override;
    kcenon::common::VoidResult initialize(const core::connection_config& config) override;
    kcenon::common::VoidResult shutdown() override;
    bool is_initialized() const override;
    kcenon::common::Result<core::database_result> select_query(const std::string& query_string) override;
    kcenon::common::VoidResult execute_query(const std::string& query_string) override;
    kcenon::common::VoidResult begin_transaction() override;
    kcenon::common::VoidResult commit_transaction() override;
    kcenon::common::VoidResult rollback_transaction() override;
    bool in_transaction() const override;
    std::string last_error() const override;
    std::map<std::string, std::string> connection_info() const override;

    // Mock configuration
    mock_database& set_database_type(database_types type);
    mock_database& set_connect_result(bool result);
    mock_database& set_default_select_result(const core::database_result& result);
    mock_database& set_default_rows_affected(uint64_t rows);

    // Expectation setting - returns expectation_builder for fluent API
    expectation_builder expect_query(const std::string& query);
    expectation_builder expect_pattern(const std::string& regex_pattern);
    expectation_builder expect_any();

    // Error simulation
    mock_database& simulate_connection_failure();
    mock_database& simulate_disconnect();

    // Verification
    bool verify_all_expectations() const;
    std::vector<std::string> get_executed_queries() const;
    size_t get_query_count() const;
    size_t get_query_count(const std::string& pattern) const;
    void reset();
    void clear_expectations();
    void clear_history();

    // State inspection
    bool is_connected() const;
    std::string get_connection_string() const;

private:
    friend class expectation_builder;

    database_types db_type_;
    bool initialized_;
    bool connect_result_;
    bool in_transaction_;
    std::string connection_string_;
    core::database_result default_result_;
    uint64_t default_rows_affected_;
    std::string last_error_;

    std::vector<expectation> expectations_;
    std::vector<std::string> executed_queries_;
    mutable std::mutex mutex_;

    void record_query(const std::string& query);
    expectation* find_expectation(const std::string& query);
};

/**
 * @class mock_database_builder
 * @brief Builder for common mock configurations
 */
class mock_database_builder {
public:
    mock_database_builder();

    // Preset configurations
    static mock_database empty_database();
    static mock_database with_data(const std::string& table_name, const core::database_result& data);
    static mock_database failing_database(const std::string& error = "Mock database error");

    // Fluent configuration
    mock_database_builder& with_type(database_types type);
    mock_database_builder& with_default_result(const core::database_result& result);
    mock_database_builder& that_fails_on_connect();

    mock_database build();

private:
    std::unique_ptr<mock_database> mock_;
};

} // namespace database::testing
