/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include "../database_types.h"
#include "value_formatter.h"
#include <string>
#include <vector>
#include <memory>

namespace database::query {

/**
 * @enum logical_op
 * @brief Logical operators for combining conditions
 */
enum class logical_op {
    AND,
    OR
};

/**
 * @struct condition
 * @brief Represents a single WHERE condition
 */
struct condition {
    std::string field;           ///< Field name
    std::string op;              ///< Operator (=, !=, <, >, <=, >=, LIKE, IN, etc.)
    database_value value;        ///< Value to compare
    std::string raw;             ///< Raw SQL condition (if any)

    bool is_raw() const { return !raw.empty(); }
};

/**
 * @class condition_builder
 * @brief Builds WHERE clause conditions with proper precedence
 *
 * Features:
 * - Supports AND/OR logical operators
 * - Nested condition grouping with parentheses
 * - Raw SQL conditions
 * - Automatic precedence handling
 *
 * Examples:
 * @code
 *   condition_builder builder;
 *
 *   // Simple condition: status = 'active'
 *   builder.add({"status", "=", std::string("active")});
 *
 *   // Complex: status = 'active' AND (age > 18 OR verified = true)
 *   builder.add({"status", "=", std::string("active")})
 *          .begin_group()
 *          .add({"age", ">", 18}, logical_op::AND)
 *          .add({"verified", "=", true}, logical_op::OR)
 *          .end_group();
 *
 *   // Build with formatter
 *   value_formatter fmt(database_types::PostgreSQL);
 *   std::string where_clause = builder.build(fmt);
 * @endcode
 *
 * Thread Safety:
 * - NOT thread-safe (stateful builder pattern)
 * - Each thread should use separate instance
 */
class condition_builder {
public:
    condition_builder();

    /**
     * @brief Add a condition
     * @param cond Condition to add
     * @param op Logical operator (AND/OR)
     * @return Reference to this builder
     */
    condition_builder& add(const condition& cond, logical_op op = logical_op::AND);

    /**
     * @brief Add a simple condition
     * @param field Field name
     * @param operator_ Comparison operator
     * @param value Value to compare
     * @param op Logical operator (AND/OR)
     * @return Reference to this builder
     */
    condition_builder& add(const std::string& field, const std::string& operator_,
                          const database_value& value, logical_op op = logical_op::AND);

    /**
     * @brief Add a raw SQL condition
     * @param raw Raw SQL condition
     * @param op Logical operator (AND/OR)
     * @return Reference to this builder
     */
    condition_builder& add_raw(const std::string& raw, logical_op op = logical_op::AND);

    /**
     * @brief Begin a condition group (adds opening parenthesis)
     * @return Reference to this builder
     */
    condition_builder& begin_group();

    /**
     * @brief End a condition group (adds closing parenthesis)
     * @return Reference to this builder
     */
    condition_builder& end_group();

    /**
     * @brief Build the WHERE clause
     * @param formatter Value formatter for this database
     * @return SQL WHERE clause (without "WHERE" keyword)
     */
    std::string build(const value_formatter& formatter) const;

    /**
     * @brief Check if builder is empty
     * @return true if no conditions added
     */
    bool empty() const;

    /**
     * @brief Clear all conditions
     */
    void clear();

private:
    struct condition_node {
        condition cond;
        logical_op op;
        int group_level;
        bool is_group_start;
        bool is_group_end;
    };

    std::vector<condition_node> nodes_;
    int current_group_level_;

    std::string format_condition(const condition& cond, const value_formatter& formatter) const;
    std::string logical_op_to_string(logical_op op) const;
};

} // namespace database::query
