// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/core/database_backend.h>
#include "mock_backend_expectations.h"
#include <queue>
#include <functional>
#include <mutex>
#include <regex>

namespace kcenon::database::testing {

/**
 * @class mock_backend
 * @brief Configurable mock for database_backend interface
 *
 * This is the modern mock class that implements database_backend
 * and returns Result<T> types for proper error handling.
 *
 * Features:
 * - Set expected queries and results with Result<T> types
 * - Simulate errors with proper error messages
 * - Record all executed queries for verification
 * - Pattern matching for flexible expectations
 * - Transaction state tracking
 *
 * Example usage:
 * @code
 *   mock_backend db;
 *
 *   // Set up expectation
 *   db.expect_query("SELECT * FROM users")
 *     .will_return(core::database_result{
 *         {{"id", int64_t(1)}, {"name", std::string("John")}}
 *     });
 *
 *   // Execute query
 *   auto result = db.select_query("SELECT * FROM users");
 *   EXPECT_TRUE(result.is_ok());
 *
 *   // Verify
 *   EXPECT_TRUE(db.verify_all_expectations());
 * @endcode
 */
class mock_backend : public core::database_backend {
public:
    mock_backend();
    ~mock_backend() override = default;

    // Non-copyable due to mutex
    mock_backend(const mock_backend&) = delete;
    mock_backend& operator=(const mock_backend&) = delete;

    // Movable
    mock_backend(mock_backend&& other) noexcept;
    mock_backend& operator=(mock_backend&& other) noexcept;

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
    mock_backend& set_database_type(database_types type);
    mock_backend& set_initialize_result(bool result, const std::string& error = "");
    mock_backend& set_default_select_result(const core::database_result& result);
    mock_backend& set_default_rows_affected(uint64_t rows);

    // Expectation setting - returns backend_expectation_builder for fluent API
    backend_expectation_builder expect_query(const std::string& query);
    backend_expectation_builder expect_pattern(const std::string& regex_pattern);
    backend_expectation_builder expect_any();

    // Error simulation
    mock_backend& simulate_initialization_failure(const std::string& error = "Mock initialization failed");
    mock_backend& simulate_shutdown();

    // Verification
    bool verify_all_expectations() const;
    std::vector<std::string> get_executed_queries() const;
    size_t get_query_count() const;
    size_t get_query_count(const std::string& pattern) const;
    void reset();
    void clear_expectations();
    void clear_history();

    // State inspection
    std::string get_connection_string() const;

private:
    friend class backend_expectation_builder;

    database_types db_type_;
    bool initialized_;
    bool init_result_;
    std::string init_error_;
    std::string connection_string_;
    core::database_result default_result_;
    uint64_t default_rows_affected_;
    std::string last_error_;
    bool in_transaction_;

    std::vector<backend_expectation> expectations_;
    std::vector<std::string> executed_queries_;
    mutable std::mutex mutex_;

    void record_query(const std::string& query);
    backend_expectation* find_expectation(const std::string& query);
};

/**
 * @class mock_backend_builder
 * @brief Builder for common mock configurations
 */
class mock_backend_builder {
public:
    mock_backend_builder();

    // Preset configurations
    static mock_backend empty_database();
    static mock_backend with_data(const std::string& table_name, const core::database_result& data);
    static mock_backend failing_database(const std::string& error = "Mock database error");

    // Fluent configuration
    mock_backend_builder& with_type(database_types type);
    mock_backend_builder& with_default_result(const core::database_result& result);
    mock_backend_builder& that_fails_on_initialize();

    mock_backend build();

private:
    std::unique_ptr<mock_backend> mock_;
};

} // namespace kcenon::database::testing
