// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "database/core/database_backend.h"
#include "mock_database.h"
#include "mock_backend.h"
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace database::testing {

/**
 * @brief Helper function to create test data rows
 *
 * Example:
 * @code
 *   auto result = make_result({
 *       {{"id", int64_t(1)}, {"name", std::string("Alice")}},
 *       {{"id", int64_t(2)}, {"name", std::string("Bob")}}
 *   });
 * @endcode
 */
inline core::database_result make_result(std::initializer_list<core::database_row> rows) {
    return core::database_result(rows);
}

/**
 * @brief Create a single row
 */
inline core::database_row make_row(std::initializer_list<std::pair<std::string, core::database_value>> fields) {
    core::database_row row;
    for (const auto& [key, value] : fields) {
        row[key] = value;
    }
    return row;
}

/**
 * @brief Create a database_value from common types
 */
template<typename T>
core::database_value make_value(const T& val) {
    return core::database_value(val);
}

/**
 * @brief Create a NULL value
 */
inline core::database_value make_null() {
    return core::database_value(nullptr);
}

/**
 * @class test_timer
 * @brief Simple timer for performance testing
 */
class test_timer {
public:
    test_timer() : start_(std::chrono::high_resolution_clock::now()) {}

    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    double elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    double elapsed_us() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(now - start_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

/**
 * @class scoped_test_database
 * @brief RAII wrapper for test database setup/teardown
 */
class scoped_test_database {
public:
    explicit scoped_test_database(mock_database& db)
        : db_(db)
    {
        core::connection_config config;
        config.host = "localhost";
        config.port = 5432;
        config.database = "test_db";
        db_.initialize(config);
    }

    ~scoped_test_database() {
        db_.shutdown();
        db_.reset();
    }

    mock_database& get() { return db_; }
    const mock_database& get() const { return db_; }

private:
    mock_database& db_;
};

/**
 * @brief Generate test data for a table
 * @param count Number of rows to generate
 * @param row_generator Function to generate each row
 * @return database_result with generated rows
 */
inline core::database_result generate_test_data(
    size_t count,
    std::function<core::database_row(size_t index)> row_generator)
{
    core::database_result result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        result.push_back(row_generator(i));
    }
    return result;
}

/**
 * @brief Generate sequential integer test data
 * @param count Number of rows
 * @param id_column Name of the ID column
 * @return database_result with sequential IDs
 */
inline core::database_result generate_sequential_ids(size_t count, const std::string& id_column = "id") {
    return generate_test_data(count, [&](size_t i) {
        return core::database_row{{id_column, static_cast<int64_t>(i + 1)}};
    });
}

/**
 * @brief Test fixture helpers
 */
namespace fixtures {

/**
 * @brief Standard user table data
 */
inline core::database_result users_data() {
    return make_result({
        {{"id", int64_t(1)}, {"name", std::string("Alice")}, {"email", std::string("alice@example.com")}, {"active", true}},
        {{"id", int64_t(2)}, {"name", std::string("Bob")}, {"email", std::string("bob@example.com")}, {"active", true}},
        {{"id", int64_t(3)}, {"name", std::string("Charlie")}, {"email", std::string("charlie@example.com")}, {"active", false}}
    });
}

/**
 * @brief Standard products table data
 */
inline core::database_result products_data() {
    return make_result({
        {{"id", int64_t(1)}, {"name", std::string("Widget")}, {"price", 19.99}, {"quantity", int64_t(100)}},
        {{"id", int64_t(2)}, {"name", std::string("Gadget")}, {"price", 29.99}, {"quantity", int64_t(50)}},
        {{"id", int64_t(3)}, {"name", std::string("Thing")}, {"price", 9.99}, {"quantity", int64_t(200)}}
    });
}

/**
 * @brief Empty result
 */
inline core::database_result empty_result() {
    return core::database_result{};
}

} // namespace fixtures

/**
 * @brief Query assertion helpers
 */
namespace assertions {

/**
 * @brief Check if result contains expected number of rows
 */
inline bool has_rows(const core::database_result& result, size_t expected) {
    return result.size() == expected;
}

/**
 * @brief Check if result is empty
 */
inline bool is_empty(const core::database_result& result) {
    return result.empty();
}

/**
 * @brief Check if result contains a row with specific field value
 */
template<typename T>
bool contains_field_value(const core::database_result& result, const std::string& field, const T& value) {
    for (const auto& row : result) {
        auto it = row.find(field);
        if (it != row.end()) {
            if (auto* v = std::get_if<T>(&it->second)) {
                if (*v == value) return true;
            }
        }
    }
    return false;
}

/**
 * @brief Check if a Result<T> is successful
 */
template<typename T>
bool is_success(const kcenon::common::Result<T>& result) {
    return result.is_ok();
}

/**
 * @brief Check if a Result<T> is an error
 */
template<typename T>
bool is_error(const kcenon::common::Result<T>& result) {
    return result.is_err();
}

/**
 * @brief Check if a VoidResult is successful
 */
inline bool is_success(const kcenon::common::VoidResult& result) {
    return result.is_ok();
}

/**
 * @brief Check if a VoidResult is an error
 */
inline bool is_error(const kcenon::common::VoidResult& result) {
    return result.is_err();
}

} // namespace assertions

} // namespace database::testing
