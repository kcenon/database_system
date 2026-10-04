// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/query_builder/condition_builder.h>
#include <sstream>

namespace kcenon::database::query {

condition_builder::condition_builder()
    : current_group_level_(0) {}

condition_builder& condition_builder::add(const condition& cond, logical_op op) {
    condition_node node;
    node.cond = cond;
    node.op = op;
    node.group_level = current_group_level_;
    node.is_group_start = false;
    node.is_group_end = false;
    nodes_.push_back(node);
    return *this;
}

condition_builder& condition_builder::add(const std::string& field, const std::string& operator_,
                                         const core::database_value& value, logical_op op) {
    condition cond;
    cond.field = field;
    cond.op = operator_;
    cond.value = value;
    return add(cond, op);
}

condition_builder& condition_builder::add_raw(const std::string& raw, logical_op op) {
    condition cond;
    cond.raw = raw;
    return add(cond, op);
}

condition_builder& condition_builder::begin_group() {
    condition_node node;
    node.group_level = current_group_level_;
    node.is_group_start = true;
    node.is_group_end = false;
    node.op = logical_op::AND;  // Default, will be overridden
    nodes_.push_back(node);

    current_group_level_++;
    return *this;
}

condition_builder& condition_builder::end_group() {
    if (current_group_level_ > 0) {
        current_group_level_--;

        condition_node node;
        node.group_level = current_group_level_;
        node.is_group_start = false;
        node.is_group_end = true;
        node.op = logical_op::AND;
        nodes_.push_back(node);
    }
    return *this;
}

std::string condition_builder::build(const value_formatter& formatter) const {
    if (nodes_.empty()) {
        return "";
    }

    std::ostringstream oss;
    bool first = true;

    for (const auto& node : nodes_) {
        if (node.is_group_start) {
            if (!first) {
                oss << " " << logical_op_to_string(node.op) << " ";
            }
            oss << "(";
            first = true;
            continue;
        }

        if (node.is_group_end) {
            oss << ")";
            first = false;
            continue;
        }

        if (!first) {
            oss << " " << logical_op_to_string(node.op) << " ";
        }

        oss << format_condition(node.cond, formatter);
        first = false;
    }

    return oss.str();
}

bool condition_builder::empty() const {
    return nodes_.empty();
}

void condition_builder::clear() {
    nodes_.clear();
    current_group_level_ = 0;
}

std::string condition_builder::format_condition(const condition& cond,
                                               const value_formatter& formatter) const {
    if (cond.is_raw()) {
        return cond.raw;
    }

    std::ostringstream oss;
    oss << formatter.escape_identifier(cond.field) << " " << cond.op << " ";

    // Handle special operators
    if (cond.op == "IS NULL" || cond.op == "IS NOT NULL") {
        // No value needed
        return formatter.escape_identifier(cond.field) + " " + cond.op;
    }

    oss << formatter.format(cond.value);
    return oss.str();
}

std::string condition_builder::logical_op_to_string(logical_op op) const {
    switch (op) {
        case logical_op::AND:
            return "AND";
        case logical_op::OR:
            return "OR";
        default:
            return "AND";
    }
}

} // namespace kcenon::database::query
