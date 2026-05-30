// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query.cppm
 * @brief C++20 module partition for database_system query components.
 *
 * This module partition exports query building components:
 * - query_builder: Universal query builder with Strategy pattern
 * - query_condition: WHERE condition representation
 * - query_dialect: Database-specific query formatting
 * - join_type: SQL join type enumeration
 * - sort_order: Sort order enumeration
 *
 * Part of the kcenon.database module.
 */

module;

// Standard library includes needed before module declaration
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// Include existing headers in the global module fragment
#include <kcenon/database/database_types.h>
#include "database/database_base.h"
#include <kcenon/database/query_builder.h>
#include <kcenon/database/query_dialect.h>

export module kcenon.database:query;

import kcenon.common;

// ============================================================================
// Query Enumerations
// ============================================================================

export namespace database {

// Re-export join type enumeration
using ::database::join_type;

// Re-export sort order enumeration
using ::database::sort_order;

} // namespace kcenon::database

// ============================================================================
// Query Condition
// ============================================================================

export namespace database {

// Re-export query condition class
using ::database::query_condition;

} // namespace kcenon::database

// ============================================================================
// Query Builder
// ============================================================================

export namespace database {

// Re-export query builder class
using ::database::query_builder;

} // namespace kcenon::database

// ============================================================================
// Query Dialect (Strategy Pattern)
// ============================================================================

export namespace database {

// Re-export query dialect interface
using ::database::query_dialect;

// Re-export dialect factory function
using ::database::create_dialect;

} // namespace kcenon::database

// ============================================================================
// SQL Dialect Implementation
// ============================================================================

export namespace database::detail {

// Re-export SQL dialect implementation
using ::database::detail::sql_dialect;

// Re-export MongoDB dialect implementation
using ::database::detail::mongodb_dialect;

// Re-export Redis dialect implementation
using ::database::detail::redis_dialect;

} // namespace database::detail
