// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file backends.cppm
 * @brief C++20 module partition for database_system backend implementations.
 *
 * This module partition exports database backend implementations:
 * - postgresql_backend: PostgreSQL database backend
 * - mysql_backend: MySQL/MariaDB database backend
 * - sqlite_backend: SQLite database backend
 * - mongodb_backend: MongoDB database backend
 * - redis_backend: Redis database backend
 *
 * Backend availability depends on compile-time configuration:
 * - USE_POSTGRESQL: Enables PostgreSQL backend
 * - USE_MYSQL: Enables MySQL backend
 * - USE_SQLITE: Enables SQLite backend
 * - USE_MONGODB: Enables MongoDB backend
 * - USE_REDIS: Enables Redis backend
 *
 * Part of the kcenon.database module.
 */

module;

// Standard library includes needed before module declaration
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// Include core headers
#include "database/database_types.h"
#include "database/core/database_backend.h"

// Conditionally include backend headers based on configuration
#ifdef USE_POSTGRESQL
#include "database/backends/postgresql_backend.h"
#endif

#ifdef USE_MYSQL
#include "database/backends/mysql_backend.h"
#endif

#ifdef USE_SQLITE
#include "database/backends/sqlite_backend.h"
#endif

#ifdef USE_MONGODB
#include "database/backends/mongodb_backend.h"
#endif

#ifdef USE_REDIS
#include "database/backends/redis_backend.h"
#endif

export module kcenon.database:backends;

import kcenon.common;

// ============================================================================
// PostgreSQL Backend
// ============================================================================

#ifdef USE_POSTGRESQL
export namespace database::backends {

// Re-export PostgreSQL backend
using ::database::backends::postgresql_backend;

} // namespace database::backends
#endif

// ============================================================================
// MySQL Backend
// ============================================================================

#ifdef USE_MYSQL
export namespace database::backends {

// Re-export MySQL backend
using ::database::backends::mysql_backend;

} // namespace database::backends
#endif

// ============================================================================
// SQLite Backend
// ============================================================================

#ifdef USE_SQLITE
export namespace database::backends {

// Re-export SQLite backend
using ::database::backends::sqlite_backend;

} // namespace database::backends
#endif

// ============================================================================
// MongoDB Backend
// ============================================================================

#ifdef USE_MONGODB
export namespace database::backends {

// Re-export MongoDB backend
using ::database::backends::mongodb_backend;

} // namespace database::backends
#endif

// ============================================================================
// Redis Backend
// ============================================================================

#ifdef USE_REDIS
export namespace database::backends {

// Re-export Redis backend
using ::database::backends::redis_backend;

} // namespace database::backends
#endif
