// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file result.h
 * @brief Result<T> type for database_system error handling
 *
 * This file re-exports common_system's error handling types for use within
 * the database module. All code should use:
 * - common::Result<T> for operations that return a value
 * - common::VoidResult for operations that don't return a value
 * - common::error_info for error information
 *
 * @see https://github.com/kcenon/common_system
 */

#include <kcenon/common/patterns/result.h>

#include <kcenon/database/compat.h>

namespace kcenon::database {

// =============================================================================
// Primary types - imported from common_system
// =============================================================================

/// @brief Primary Result type - use this for all database operations
template<typename T>
using Result = kcenon::common::Result<T>;

/// @brief Primary VoidResult type - use this for void operations
using VoidResult = kcenon::common::VoidResult;

/// @brief Primary error type
using error_info = kcenon::common::error_info;

// =============================================================================
// Database-specific error codes
// =============================================================================

/// Error codes for database operations
enum class error_code {
	success = 0,
	unknown_error = -1,
	invalid_argument = -2,
	not_implemented = -3,
	invalid_state = -4,
	connection_failed = -5,
	query_failed = -6,
	timeout = -7
};

} // namespace kcenon::database

// =============================================================================
// Integrated namespace for compatibility
// =============================================================================

namespace kcenon::database::integrated {
	using database::Result;
	using database::VoidResult;
	using database::error_info;
	using database::error_code;
} // namespace kcenon::database::integrated
