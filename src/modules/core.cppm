// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file core.cppm
 * @brief C++20 module partition for database_system core components.
 *
 * This module partition exports core database components:
 * - database_types: Database type enumeration
 * - connection_mode: Connection mode enumeration
 * - database_backend: Abstract backend interface
 * - connection_config: Connection configuration
 * - database_context: Dependency injection context
 * - backend_registry: Backend factory registry
 * - database_manager: High-level database manager
 *
 * Part of the kcenon.database module.
 */

module;

// Standard library includes needed before module declaration
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Include existing headers in the global module fragment
#include <kcenon/database/database_types.h>
#include "database/database_base.h"
#include <kcenon/database/core/database_backend.h>
#include <kcenon/database/core/database_context.h>
#include <kcenon/database/core/backend_registry.h>
#include <kcenon/database/core/concepts.h>
#include <kcenon/database/database_manager.h>
#include "database/proxy/proxy_config.h"

export module kcenon.database:core;

import kcenon.common;

// ============================================================================
// Core Type Aliases
// ============================================================================

export namespace database {

/// @brief Variant type for individual database cell values
using core::database_value;
/// @brief A single row of database values (ordered by column)
using core::database_row;
/// @brief Collection of rows returned from a database query
using core::database_result;

} // namespace kcenon::database

// ============================================================================
// Database Types Enumeration
// ============================================================================

export namespace database {

// Re-export database types enumeration
using ::database::database_types;
using ::database::connection_mode;

// Re-export to_string functions
using ::database::to_string;

} // namespace kcenon::database

// ============================================================================
// Connection Configuration
// ============================================================================

export namespace database::core {

// Re-export connection configuration
using ::database::core::connection_config;

} // namespace database::core

// ============================================================================
// Backend Interface
// ============================================================================

export namespace database::core {

// Re-export backend interface
using ::database::core::database_backend;
using ::database::core::backend_factory_fn;

} // namespace database::core

// ============================================================================
// Backend Registry
// ============================================================================

export namespace database::core {

// Re-export backend registry
using ::database::core::backend_registry;

} // namespace database::core

// ============================================================================
// Database Context (Dependency Injection)
// ============================================================================

export namespace database {

// Re-export database context
using ::database::database_context;

} // namespace kcenon::database

// ============================================================================
// Proxy Configuration
// ============================================================================

export namespace database::proxy {

// Re-export proxy configuration
using ::database::proxy::proxy_connection_config;

} // namespace database::proxy

// ============================================================================
// Database Manager
// ============================================================================

export namespace database {

// Re-export database manager
using ::database::database_manager;

} // namespace kcenon::database

// ============================================================================
// Legacy Types (deprecated)
// ============================================================================

export namespace database {

// Re-export legacy database_base (deprecated)
using ::database::database_base;

// Re-export legacy type aliases
using ::database::database_value;
using ::database::database_row;
using ::database::database_result;

} // namespace kcenon::database
