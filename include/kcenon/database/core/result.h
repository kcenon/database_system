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

/// @brief Error codes for database operations.
///
/// All non-zero values live in common_system's reserved database_system band
/// [-599, -500] (see kcenon::common::error::category::database_system). This
/// guarantees that when a backend casts one of these into the shared
/// kcenon::common::error_info::code, consumers calling
/// kcenon::common::error::get_category_name() / get_error_message() resolve the
/// "DatabaseSystem" category instead of mis-attributing the code to the common
/// (-1..-99) band.
///
/// Where common already defines a matching database code in
/// kcenon::common::error::codes::database_system, the enumerator is aligned to
/// that exact value so the shared message table returns the correct text:
///   - connection_failed -> database_system::connection_failed (base-0  = -500)
///   - query_failed      -> database_system::query_failed      (base-40 = -540)
///   - timeout           -> database_system::query_timeout     (base-42 = -542)
///
/// Database-specific generics with no common equivalent occupy distinct unused
/// slots at the tail of the band; none collide with a common database_system
/// code or with the adapter-local constants in
/// common_system_database_adapter.h (-580..-585).
enum class error_code {
	success = 0,
	connection_failed = -500, // == common::error::codes::database_system::connection_failed
	invalid_state = -596,
	not_implemented = -597,
	invalid_argument = -598,
	unknown_error = -599,
	query_failed = -540,      // == common::error::codes::database_system::query_failed
	timeout = -542            // == common::error::codes::database_system::query_timeout
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
