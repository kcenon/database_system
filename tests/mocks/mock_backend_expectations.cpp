/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, Database System
All rights reserved.
*****************************************************************************/

#include "mock_backend_expectations.h"
#include "mock_backend.h"

namespace database::testing {

// backend_expectation implementation
backend_expectation::backend_expectation()
    : match_type_(backend_match_type::ANY)
    , execute_success_(true)
    , min_invocations_(0)
    , max_invocations_(std::numeric_limits<int>::max())
    , actual_invocations_(0)
{
}

backend_expectation& backend_expectation::for_query(const std::string& query, backend_match_type type) {
    query_ = query;
    match_type_ = type;
    if (type == backend_match_type::PATTERN) {
        pattern_ = std::regex(query);
    }
    return *this;
}

backend_expectation& backend_expectation::for_pattern(const std::string& pattern) {
    return for_query(pattern, backend_match_type::PATTERN);
}

backend_expectation& backend_expectation::for_any() {
    match_type_ = backend_match_type::ANY;
    return *this;
}

backend_expectation& backend_expectation::returning(const core::database_result& result) {
    result_ = result;
    return *this;
}

backend_expectation& backend_expectation::returning_rows_affected(uint64_t count) {
    rows_affected_ = count;
    return *this;
}

backend_expectation& backend_expectation::returning_error(const std::string& error_message) {
    error_message_ = error_message;
    return *this;
}

backend_expectation& backend_expectation::returning_execute_success() {
    execute_success_ = true;
    error_message_.reset();
    return *this;
}

backend_expectation& backend_expectation::times(int count) {
    min_invocations_ = count;
    max_invocations_ = count;
    return *this;
}

backend_expectation& backend_expectation::at_least(int count) {
    min_invocations_ = count;
    return *this;
}

backend_expectation& backend_expectation::at_most(int count) {
    max_invocations_ = count;
    return *this;
}

backend_expectation& backend_expectation::once() {
    return times(1);
}

backend_expectation& backend_expectation::never() {
    return times(0);
}

bool backend_expectation::matches(const std::string& query) const {
    switch (match_type_) {
        case backend_match_type::EXACT:
            return query == query_;
        case backend_match_type::PATTERN:
            return std::regex_search(query, pattern_);
        case backend_match_type::ANY:
            return true;
        default:
            return false;
    }
}

bool backend_expectation::is_satisfied() const {
    return actual_invocations_ >= min_invocations_;
}

bool backend_expectation::can_be_invoked() const {
    return actual_invocations_ < max_invocations_;
}

kcenon::common::Result<core::database_result> backend_expectation::get_select_result() {
    ++actual_invocations_;
    if (error_message_) {
        return kcenon::common::Result<core::database_result>(kcenon::common::error_info{-1, *error_message_});
    }
    return kcenon::common::Result<core::database_result>::ok(result_.value_or(core::database_result{}));
}

kcenon::common::Result<uint64_t> backend_expectation::get_rows_affected() {
    ++actual_invocations_;
    if (error_message_) {
        return kcenon::common::Result<uint64_t>(kcenon::common::error_info{-1, *error_message_});
    }
    return kcenon::common::Result<uint64_t>::ok(rows_affected_.value_or(0));
}

kcenon::common::VoidResult backend_expectation::get_execute_result() {
    ++actual_invocations_;
    if (error_message_) {
        return kcenon::common::VoidResult(kcenon::common::error_info{-1, *error_message_});
    }
    return kcenon::common::VoidResult(std::monostate{});
}

bool backend_expectation::should_error() const {
    return error_message_.has_value();
}

std::string backend_expectation::get_error_message() const {
    return error_message_.value_or("");
}

// backend_expectation_builder implementation
backend_expectation_builder::backend_expectation_builder(mock_backend* db, backend_expectation exp)
    : db_(db), exp_(std::move(exp))
{
}

backend_expectation_builder& backend_expectation_builder::will_return(const core::database_result& result) {
    exp_.returning(result);
    db_->expectations_.push_back(exp_);
    return *this;
}

backend_expectation_builder& backend_expectation_builder::will_return_rows(uint64_t count) {
    exp_.returning_rows_affected(count);
    db_->expectations_.push_back(exp_);
    return *this;
}

backend_expectation_builder& backend_expectation_builder::will_fail(const std::string& error_message) {
    exp_.returning_error(error_message);
    db_->expectations_.push_back(exp_);
    return *this;
}

backend_expectation_builder& backend_expectation_builder::will_succeed() {
    exp_.returning_execute_success();
    db_->expectations_.push_back(exp_);
    return *this;
}

backend_expectation_builder& backend_expectation_builder::times(int count) {
    exp_.times(count);
    return *this;
}

backend_expectation_builder& backend_expectation_builder::once() {
    exp_.once();
    return *this;
}

backend_expectation_builder& backend_expectation_builder::any_times() {
    exp_.at_least(0);
    return *this;
}

} // namespace database::testing
