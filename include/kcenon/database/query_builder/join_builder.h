// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <kcenon/database/database_types.h>
#include <kcenon/database/query_builder/value_formatter.h>
#include <string>
#include <vector>

namespace database::query {

/**
 * @enum join_type
 * @brief Types of SQL joins
 */
enum class join_type {
    inner,      ///< INNER JOIN
    left,       ///< LEFT OUTER JOIN
    right,      ///< RIGHT OUTER JOIN
    full_outer, ///< FULL OUTER JOIN
    cross       ///< CROSS JOIN
};

/**
 * @struct join_spec
 * @brief Specification for a single JOIN clause
 */
struct join_spec {
    std::string table;       ///< Table to join
    std::string condition;   ///< ON condition
    join_type type;          ///< Type of join
    std::string alias;       ///< Optional table alias
};

/**
 * @class join_builder
 * @brief Builds JOIN clauses for SQL queries
 *
 * Features:
 * - Multiple JOIN types (INNER, LEFT, RIGHT, FULL OUTER, CROSS)
 * - Table aliasing
 * - Multiple joins chaining
 *
 * Example:
 * @code
 *   join_builder builder;
 *
 *   // Simple join
 *   builder.join("orders", "users.id = orders.user_id");
 *
 *   // Left join with alias
 *   builder.left_join("products", "orders.product_id = products.id", "p");
 *
 *   // Build the JOIN clauses
 *   value_formatter fmt(database_types::postgres);
 *   std::string joins = builder.build(fmt);
 *   // Result: "INNER JOIN orders ON users.id = orders.user_id
 *   //          LEFT JOIN products AS p ON orders.product_id = products.id"
 * @endcode
 *
 * Thread Safety:
 * - NOT thread-safe (stateful builder pattern)
 * - Each thread should use separate instance
 */
class join_builder {
public:
    join_builder();

    /**
     * @brief Add an INNER JOIN
     * @param table Table to join
     * @param condition ON condition
     * @param alias Optional table alias
     * @return Reference to this builder
     */
    join_builder& join(const std::string& table, const std::string& condition,
                       const std::string& alias = "");

    /**
     * @brief Add a LEFT JOIN
     * @param table Table to join
     * @param condition ON condition
     * @param alias Optional table alias
     * @return Reference to this builder
     */
    join_builder& left_join(const std::string& table, const std::string& condition,
                            const std::string& alias = "");

    /**
     * @brief Add a RIGHT JOIN
     * @param table Table to join
     * @param condition ON condition
     * @param alias Optional table alias
     * @return Reference to this builder
     */
    join_builder& right_join(const std::string& table, const std::string& condition,
                             const std::string& alias = "");

    /**
     * @brief Add a FULL OUTER JOIN
     * @param table Table to join
     * @param condition ON condition
     * @param alias Optional table alias
     * @return Reference to this builder
     */
    join_builder& full_outer_join(const std::string& table, const std::string& condition,
                                   const std::string& alias = "");

    /**
     * @brief Add a CROSS JOIN
     * @param table Table to join
     * @param alias Optional table alias
     * @return Reference to this builder
     */
    join_builder& cross_join(const std::string& table, const std::string& alias = "");

    /**
     * @brief Add a join with explicit type
     * @param spec Join specification
     * @return Reference to this builder
     */
    join_builder& add(const join_spec& spec);

    /**
     * @brief Build the JOIN clauses
     * @param formatter Value formatter for escaping identifiers
     * @return SQL JOIN clauses
     */
    std::string build(const value_formatter& formatter) const;

    /**
     * @brief Check if builder is empty
     * @return true if no joins added
     */
    bool empty() const;

    /**
     * @brief Clear all joins
     */
    void clear();

    /**
     * @brief Get number of joins
     * @return Number of joins added
     */
    size_t size() const;

private:
    std::vector<join_spec> joins_;

    std::string join_type_to_string(join_type type) const;
};

} // namespace database::query
