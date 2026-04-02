// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "join_builder.h"
#include <sstream>

namespace database::query {

join_builder::join_builder() = default;

join_builder& join_builder::join(const std::string& table, const std::string& condition,
                                  const std::string& alias) {
    joins_.push_back({table, condition, join_type::inner, alias});
    return *this;
}

join_builder& join_builder::left_join(const std::string& table, const std::string& condition,
                                       const std::string& alias) {
    joins_.push_back({table, condition, join_type::left, alias});
    return *this;
}

join_builder& join_builder::right_join(const std::string& table, const std::string& condition,
                                        const std::string& alias) {
    joins_.push_back({table, condition, join_type::right, alias});
    return *this;
}

join_builder& join_builder::full_outer_join(const std::string& table, const std::string& condition,
                                             const std::string& alias) {
    joins_.push_back({table, condition, join_type::full_outer, alias});
    return *this;
}

join_builder& join_builder::cross_join(const std::string& table, const std::string& alias) {
    joins_.push_back({table, "", join_type::cross, alias});
    return *this;
}

join_builder& join_builder::add(const join_spec& spec) {
    joins_.push_back(spec);
    return *this;
}

std::string join_builder::build(const value_formatter& formatter) const {
    if (joins_.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < joins_.size(); ++i) {
        const auto& join = joins_[i];

        if (i > 0) {
            oss << " ";
        }

        oss << join_type_to_string(join.type) << " JOIN ";
        oss << formatter.escape_identifier(join.table);

        if (!join.alias.empty()) {
            oss << " AS " << formatter.escape_identifier(join.alias);
        }

        if (join.type != join_type::cross && !join.condition.empty()) {
            oss << " ON " << join.condition;
        }
    }

    return oss.str();
}

bool join_builder::empty() const {
    return joins_.empty();
}

void join_builder::clear() {
    joins_.clear();
}

size_t join_builder::size() const {
    return joins_.size();
}

std::string join_builder::join_type_to_string(join_type type) const {
    switch (type) {
        case join_type::inner:
            return "INNER";
        case join_type::left:
            return "LEFT";
        case join_type::right:
            return "RIGHT";
        case join_type::full_outer:
            return "FULL OUTER";
        case join_type::cross:
            return "CROSS";
        default:
            return "INNER";
    }
}

} // namespace database::query
