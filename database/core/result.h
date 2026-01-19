// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

namespace database {

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

} // namespace database

// =============================================================================
// Integrated namespace for compatibility
// =============================================================================

namespace database::integrated {
	using database::Result;
	using database::VoidResult;
	using database::error_info;
	using database::error_code;
} // namespace database::integrated
