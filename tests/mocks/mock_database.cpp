/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, Database System
All rights reserved.
*****************************************************************************/

#include "mock_database.h"
#include <algorithm>

namespace database::testing {

// mock_database implementation
mock_database::mock_database()
    : db_type_(database_types::none)
    , initialized_(false)
    , connect_result_(true)
    , in_transaction_(false)
    , default_rows_affected_(1)
{
}

mock_database::mock_database(mock_database&& other) noexcept
    : db_type_(other.db_type_)
    , initialized_(other.initialized_)
    , connect_result_(other.connect_result_)
    , in_transaction_(other.in_transaction_)
    , connection_string_(std::move(other.connection_string_))
    , default_result_(std::move(other.default_result_))
    , default_rows_affected_(other.default_rows_affected_)
    , last_error_(std::move(other.last_error_))
    , expectations_(std::move(other.expectations_))
    , executed_queries_(std::move(other.executed_queries_))
{
    other.initialized_ = false;
    other.in_transaction_ = false;
}

mock_database& mock_database::operator=(mock_database&& other) noexcept {
    if (this != &other) {
        db_type_ = other.db_type_;
        initialized_ = other.initialized_;
        connect_result_ = other.connect_result_;
        in_transaction_ = other.in_transaction_;
        connection_string_ = std::move(other.connection_string_);
        default_result_ = std::move(other.default_result_);
        default_rows_affected_ = other.default_rows_affected_;
        last_error_ = std::move(other.last_error_);
        expectations_ = std::move(other.expectations_);
        executed_queries_ = std::move(other.executed_queries_);
        other.initialized_ = false;
        other.in_transaction_ = false;
    }
    return *this;
}

database_types mock_database::type() const {
    return db_type_;
}

kcenon::common::VoidResult mock_database::initialize(const core::connection_config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_string_ = config.host + ":" + std::to_string(config.port) + "/" + config.database;
    if (connect_result_) {
        initialized_ = true;
        return kcenon::common::VoidResult::ok(std::monostate{});
    }
    last_error_ = "Connection failed";
    return kcenon::common::VoidResult::err(kcenon::common::error_info{"Connection failed"});
}

kcenon::common::VoidResult mock_database::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    in_transaction_ = false;
    return kcenon::common::VoidResult::ok(std::monostate{});
}

bool mock_database::is_initialized() const {
    return initialized_;
}

kcenon::common::Result<core::database_result> mock_database::select_query(const std::string& query_string) {
    std::lock_guard<std::mutex> lock(mutex_);
    record_query(query_string);

    if (auto* exp = find_expectation(query_string)) {
        if (exp->should_throw()) {
            last_error_ = exp->get_error_message();
            return kcenon::common::Result<core::database_result>::err(kcenon::common::error_info{last_error_});
        }
        return kcenon::common::Result<core::database_result>::ok(exp->get_result());
    }
    return kcenon::common::Result<core::database_result>::ok(default_result_);
}

kcenon::common::VoidResult mock_database::execute_query(const std::string& query_string) {
    std::lock_guard<std::mutex> lock(mutex_);
    record_query(query_string);

    if (auto* exp = find_expectation(query_string)) {
        if (exp->should_throw()) {
            last_error_ = exp->get_error_message();
            return kcenon::common::VoidResult::err(kcenon::common::error_info{last_error_});
        }
        if (!exp->get_execute_result()) {
            last_error_ = "Query execution failed";
            return kcenon::common::VoidResult::err(kcenon::common::error_info{last_error_});
        }
    }
    return kcenon::common::VoidResult::ok(std::monostate{});
}

kcenon::common::VoidResult mock_database::begin_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (in_transaction_) {
        return kcenon::common::VoidResult::err(kcenon::common::error_info{"Transaction already active"});
    }
    in_transaction_ = true;
    return kcenon::common::VoidResult::ok(std::monostate{});
}

kcenon::common::VoidResult mock_database::commit_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!in_transaction_) {
        return kcenon::common::VoidResult::err(kcenon::common::error_info{"No active transaction"});
    }
    in_transaction_ = false;
    return kcenon::common::VoidResult::ok(std::monostate{});
}

kcenon::common::VoidResult mock_database::rollback_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!in_transaction_) {
        return kcenon::common::VoidResult::err(kcenon::common::error_info{"No active transaction"});
    }
    in_transaction_ = false;
    return kcenon::common::VoidResult::ok(std::monostate{});
}

bool mock_database::in_transaction() const {
    return in_transaction_;
}

std::string mock_database::last_error() const {
    return last_error_;
}

std::map<std::string, std::string> mock_database::connection_info() const {
    std::map<std::string, std::string> info;
    info["connection_string"] = connection_string_;
    info["initialized"] = initialized_ ? "true" : "false";
    info["in_transaction"] = in_transaction_ ? "true" : "false";
    return info;
}

mock_database& mock_database::set_database_type(database_types type) {
    db_type_ = type;
    return *this;
}

mock_database& mock_database::set_connect_result(bool result) {
    connect_result_ = result;
    return *this;
}

mock_database& mock_database::set_default_select_result(const core::database_result& result) {
    default_result_ = result;
    return *this;
}

mock_database& mock_database::set_default_rows_affected(uint64_t rows) {
    default_rows_affected_ = rows;
    return *this;
}

expectation_builder mock_database::expect_query(const std::string& query) {
    expectation exp;
    exp.for_query(query, match_type::EXACT);
    return expectation_builder(this, std::move(exp));
}

expectation_builder mock_database::expect_pattern(const std::string& regex_pattern) {
    expectation exp;
    exp.for_pattern(regex_pattern);
    return expectation_builder(this, std::move(exp));
}

expectation_builder mock_database::expect_any() {
    expectation exp;
    exp.for_any();
    return expectation_builder(this, std::move(exp));
}

mock_database& mock_database::simulate_connection_failure() {
    connect_result_ = false;
    return *this;
}

mock_database& mock_database::simulate_disconnect() {
    initialized_ = false;
    return *this;
}

bool mock_database::verify_all_expectations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::all_of(expectations_.begin(), expectations_.end(),
                       [](const expectation& exp) { return exp.is_satisfied(); });
}

std::vector<std::string> mock_database::get_executed_queries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executed_queries_;
}

size_t mock_database::get_query_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executed_queries_.size();
}

size_t mock_database::get_query_count(const std::string& pattern) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::regex re(pattern);
    return std::count_if(executed_queries_.begin(), executed_queries_.end(),
                         [&re](const std::string& q) { return std::regex_search(q, re); });
}

void mock_database::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    expectations_.clear();
    executed_queries_.clear();
    initialized_ = false;
    in_transaction_ = false;
    connection_string_.clear();
    last_error_.clear();
}

void mock_database::clear_expectations() {
    std::lock_guard<std::mutex> lock(mutex_);
    expectations_.clear();
}

void mock_database::clear_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    executed_queries_.clear();
}

bool mock_database::is_connected() const {
    return initialized_;
}

std::string mock_database::get_connection_string() const {
    return connection_string_;
}

void mock_database::record_query(const std::string& query) {
    executed_queries_.push_back(query);
}

expectation* mock_database::find_expectation(const std::string& query) {
    for (auto& exp : expectations_) {
        if (exp.matches(query) && exp.can_be_invoked()) {
            return &exp;
        }
    }
    return nullptr;
}

// mock_database_builder implementation
mock_database_builder::mock_database_builder()
    : mock_(std::make_unique<mock_database>())
{
}

mock_database mock_database_builder::empty_database() {
    return mock_database();
}

mock_database mock_database_builder::with_data(const std::string& table_name, const core::database_result& data) {
    mock_database db;
    db.expect_pattern("SELECT.*FROM.*" + table_name).will_return(data);
    return db;
}

mock_database mock_database_builder::failing_database(const std::string& error) {
    mock_database db;
    db.expect_any().will_fail(error);
    return db;
}

mock_database_builder& mock_database_builder::with_type(database_types type) {
    mock_->set_database_type(type);
    return *this;
}

mock_database_builder& mock_database_builder::with_default_result(const core::database_result& result) {
    mock_->set_default_select_result(result);
    return *this;
}

mock_database_builder& mock_database_builder::that_fails_on_connect() {
    mock_->simulate_connection_failure();
    return *this;
}

mock_database mock_database_builder::build() {
    return std::move(*mock_);
}

} // namespace database::testing
