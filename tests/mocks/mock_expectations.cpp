// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "mock_expectations.h"
#include "mock_database.h"
#include <cstdint>

namespace kcenon::database::testing {

// expectation implementation
expectation::expectation()
    : match_type_(match_type::ANY)
    , min_invocations_(0)
    , max_invocations_(std::numeric_limits<int>::max())
    , actual_invocations_(0)
{
}

expectation& expectation::for_query(const std::string& query, match_type type) {
    query_ = query;
    match_type_ = type;
    if (type == match_type::PATTERN) {
        pattern_ = std::regex(query);
    }
    return *this;
}

expectation& expectation::for_pattern(const std::string& pattern) {
    return for_query(pattern, match_type::PATTERN);
}

expectation& expectation::for_any() {
    match_type_ = match_type::ANY;
    return *this;
}

expectation& expectation::returning(const core::database_result& result) {
    result_ = result;
    return *this;
}

expectation& expectation::returning_rows_affected(uint64_t count) {
    rows_affected_ = count;
    return *this;
}

expectation& expectation::throwing(const std::string& error_message) {
    error_message_ = error_message;
    return *this;
}

expectation& expectation::returning_execute_result(bool result) {
    execute_result_ = result;
    return *this;
}

expectation& expectation::times(int count) {
    min_invocations_ = count;
    max_invocations_ = count;
    return *this;
}

expectation& expectation::at_least(int count) {
    min_invocations_ = count;
    return *this;
}

expectation& expectation::at_most(int count) {
    max_invocations_ = count;
    return *this;
}

expectation& expectation::once() {
    return times(1);
}

expectation& expectation::never() {
    return times(0);
}

bool expectation::matches(const std::string& query) const {
    switch (match_type_) {
        case match_type::EXACT:
            return query == query_;
        case match_type::PATTERN:
            return std::regex_search(query, pattern_);
        case match_type::ANY:
            return true;
        default:
            return false;
    }
}

bool expectation::is_satisfied() const {
    return actual_invocations_ >= min_invocations_;
}

bool expectation::can_be_invoked() const {
    return actual_invocations_ < max_invocations_;
}

core::database_result expectation::get_result() {
    ++actual_invocations_;
    if (error_message_) {
        throw database_exception(*error_message_);
    }
    return result_.value_or(core::database_result{});
}

uint64_t expectation::get_rows_affected() {
    ++actual_invocations_;
    if (error_message_) {
        throw database_exception(*error_message_);
    }
    return rows_affected_.value_or(0);
}

bool expectation::get_execute_result() {
    ++actual_invocations_;
    if (error_message_) {
        throw database_exception(*error_message_);
    }
    return execute_result_.value_or(true);
}

bool expectation::should_throw() const {
    return error_message_.has_value();
}

std::string expectation::get_error_message() const {
    return error_message_.value_or("");
}

// expectation_builder implementation
expectation_builder::expectation_builder(mock_database* db, expectation exp)
    : db_(db), exp_(std::move(exp))
{
}

expectation_builder& expectation_builder::will_return(const core::database_result& result) {
    exp_.returning(result);
    db_->expectations_.push_back(exp_);
    return *this;
}

expectation_builder& expectation_builder::will_return_rows(uint64_t count) {
    exp_.returning_rows_affected(count);
    db_->expectations_.push_back(exp_);
    return *this;
}

expectation_builder& expectation_builder::will_fail(const std::string& error_message) {
    exp_.throwing(error_message);
    db_->expectations_.push_back(exp_);
    return *this;
}

expectation_builder& expectation_builder::will_succeed() {
    exp_.returning_execute_result(true);
    db_->expectations_.push_back(exp_);
    return *this;
}

expectation_builder& expectation_builder::times(int count) {
    exp_.times(count);
    return *this;
}

expectation_builder& expectation_builder::once() {
    exp_.once();
    return *this;
}

expectation_builder& expectation_builder::any_times() {
    exp_.at_least(0);
    return *this;
}

} // namespace kcenon::database::testing
