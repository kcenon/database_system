/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, Database System
All rights reserved.
*****************************************************************************/

#include "mock_backend.h"
#include <algorithm>

namespace database::testing {

// mock_backend implementation
mock_backend::mock_backend()
    : db_type_(database_types::none)
    , initialized_(false)
    , init_result_(true)
    , default_rows_affected_(1)
    , in_transaction_(false)
{
}

mock_backend::mock_backend(mock_backend&& other) noexcept
    : db_type_(other.db_type_)
    , initialized_(other.initialized_)
    , init_result_(other.init_result_)
    , init_error_(std::move(other.init_error_))
    , connection_string_(std::move(other.connection_string_))
    , default_result_(std::move(other.default_result_))
    , default_rows_affected_(other.default_rows_affected_)
    , last_error_(std::move(other.last_error_))
    , in_transaction_(other.in_transaction_)
    , expectations_(std::move(other.expectations_))
    , executed_queries_(std::move(other.executed_queries_))
{
    other.initialized_ = false;
    other.in_transaction_ = false;
}

mock_backend& mock_backend::operator=(mock_backend&& other) noexcept {
    if (this != &other) {
        db_type_ = other.db_type_;
        initialized_ = other.initialized_;
        init_result_ = other.init_result_;
        init_error_ = std::move(other.init_error_);
        connection_string_ = std::move(other.connection_string_);
        default_result_ = std::move(other.default_result_);
        default_rows_affected_ = other.default_rows_affected_;
        last_error_ = std::move(other.last_error_);
        in_transaction_ = other.in_transaction_;
        expectations_ = std::move(other.expectations_);
        executed_queries_ = std::move(other.executed_queries_);
        other.initialized_ = false;
        other.in_transaction_ = false;
    }
    return *this;
}

database_types mock_backend::type() const {
    return db_type_;
}

kcenon::common::VoidResult mock_backend::initialize(const core::connection_config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_string_ = config.host + ":" + std::to_string(config.port) + "/" + config.database;
    if (init_result_) {
        initialized_ = true;
        return kcenon::common::VoidResult(std::monostate{});
    }
    last_error_ = init_error_.empty() ? "Mock initialization failed" : init_error_;
    return kcenon::common::VoidResult(kcenon::common::error_info{-1, last_error_});
}

kcenon::common::VoidResult mock_backend::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    in_transaction_ = false;
    return kcenon::common::VoidResult(std::monostate{});
}

bool mock_backend::is_initialized() const {
    return initialized_;
}

kcenon::common::Result<core::database_result> mock_backend::select_query(const std::string& query_string) {
    std::lock_guard<std::mutex> lock(mutex_);
    record_query(query_string);

    if (auto* exp = find_expectation(query_string)) {
        return exp->get_select_result();
    }
    return kcenon::common::Result<core::database_result>::ok(default_result_);
}

kcenon::common::VoidResult mock_backend::execute_query(const std::string& query_string) {
    std::lock_guard<std::mutex> lock(mutex_);
    record_query(query_string);

    if (auto* exp = find_expectation(query_string)) {
        return exp->get_execute_result();
    }
    return kcenon::common::VoidResult(std::monostate{});
}

kcenon::common::VoidResult mock_backend::begin_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (in_transaction_) {
        last_error_ = "Transaction already in progress";
        return kcenon::common::VoidResult(kcenon::common::error_info{-1, last_error_});
    }
    in_transaction_ = true;
    return kcenon::common::VoidResult(std::monostate{});
}

kcenon::common::VoidResult mock_backend::commit_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!in_transaction_) {
        last_error_ = "No transaction in progress";
        return kcenon::common::VoidResult(kcenon::common::error_info{-1, last_error_});
    }
    in_transaction_ = false;
    return kcenon::common::VoidResult(std::monostate{});
}

kcenon::common::VoidResult mock_backend::rollback_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!in_transaction_) {
        last_error_ = "No transaction in progress";
        return kcenon::common::VoidResult(kcenon::common::error_info{-1, last_error_});
    }
    in_transaction_ = false;
    return kcenon::common::VoidResult(std::monostate{});
}

bool mock_backend::in_transaction() const {
    return in_transaction_;
}

std::string mock_backend::last_error() const {
    return last_error_;
}

std::map<std::string, std::string> mock_backend::connection_info() const {
    return {
        {"type", "mock"},
        {"connection_string", connection_string_},
        {"initialized", initialized_ ? "true" : "false"}
    };
}

mock_backend& mock_backend::set_database_type(database_types type) {
    db_type_ = type;
    return *this;
}

mock_backend& mock_backend::set_initialize_result(bool result, const std::string& error) {
    init_result_ = result;
    init_error_ = error;
    return *this;
}

mock_backend& mock_backend::set_default_select_result(const core::database_result& result) {
    default_result_ = result;
    return *this;
}

mock_backend& mock_backend::set_default_rows_affected(uint64_t rows) {
    default_rows_affected_ = rows;
    return *this;
}

backend_expectation_builder mock_backend::expect_query(const std::string& query) {
    backend_expectation exp;
    exp.for_query(query, backend_match_type::EXACT);
    return backend_expectation_builder(this, std::move(exp));
}

backend_expectation_builder mock_backend::expect_pattern(const std::string& regex_pattern) {
    backend_expectation exp;
    exp.for_pattern(regex_pattern);
    return backend_expectation_builder(this, std::move(exp));
}

backend_expectation_builder mock_backend::expect_any() {
    backend_expectation exp;
    exp.for_any();
    return backend_expectation_builder(this, std::move(exp));
}

mock_backend& mock_backend::simulate_initialization_failure(const std::string& error) {
    init_result_ = false;
    init_error_ = error;
    return *this;
}

mock_backend& mock_backend::simulate_shutdown() {
    initialized_ = false;
    return *this;
}

bool mock_backend::verify_all_expectations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::all_of(expectations_.begin(), expectations_.end(),
                       [](const backend_expectation& exp) { return exp.is_satisfied(); });
}

std::vector<std::string> mock_backend::get_executed_queries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executed_queries_;
}

size_t mock_backend::get_query_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executed_queries_.size();
}

size_t mock_backend::get_query_count(const std::string& pattern) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::regex re(pattern);
    return std::count_if(executed_queries_.begin(), executed_queries_.end(),
                         [&re](const std::string& q) { return std::regex_search(q, re); });
}

void mock_backend::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    expectations_.clear();
    executed_queries_.clear();
    initialized_ = false;
    in_transaction_ = false;
    connection_string_.clear();
    last_error_.clear();
}

void mock_backend::clear_expectations() {
    std::lock_guard<std::mutex> lock(mutex_);
    expectations_.clear();
}

void mock_backend::clear_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    executed_queries_.clear();
}

std::string mock_backend::get_connection_string() const {
    return connection_string_;
}

void mock_backend::record_query(const std::string& query) {
    executed_queries_.push_back(query);
}

backend_expectation* mock_backend::find_expectation(const std::string& query) {
    for (auto& exp : expectations_) {
        if (exp.matches(query) && exp.can_be_invoked()) {
            return &exp;
        }
    }
    return nullptr;
}

// mock_backend_builder implementation
mock_backend_builder::mock_backend_builder()
    : mock_(std::make_unique<mock_backend>())
{
}

mock_backend mock_backend_builder::empty_database() {
    return mock_backend();
}

mock_backend mock_backend_builder::with_data(const std::string& table_name, const core::database_result& data) {
    mock_backend db;
    db.expect_pattern("SELECT.*FROM.*" + table_name).will_return(data);
    return db;
}

mock_backend mock_backend_builder::failing_database(const std::string& error) {
    mock_backend db;
    db.expect_any().will_fail(error);
    return db;
}

mock_backend_builder& mock_backend_builder::with_type(database_types type) {
    mock_->set_database_type(type);
    return *this;
}

mock_backend_builder& mock_backend_builder::with_default_result(const core::database_result& result) {
    mock_->set_default_select_result(result);
    return *this;
}

mock_backend_builder& mock_backend_builder::that_fails_on_initialize() {
    mock_->simulate_initialization_failure();
    return *this;
}

mock_backend mock_backend_builder::build() {
    return std::move(*mock_);
}

} // namespace database::testing
