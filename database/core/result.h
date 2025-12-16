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

#include <variant>
#include <string>
#include <stdexcept>

/**
 * @file result.h
 * @brief Result<T> type for database_system error handling
 *
 * This file provides unified error handling via common_system integration.
 * The primary types to use are:
 * - common::Result<T> for operations that return a value
 * - common::VoidResult for operations that don't return a value
 * - common::error_info for error information
 *
 * MIGRATION NOTICE:
 * The database::result<T> wrapper class is DEPRECATED. Please migrate to
 * using common::Result<T> directly. The wrapper is maintained only for
 * backward compatibility and will be removed in a future version.
 *
 * Migration guide:
 * - Replace database::result<T> with common::Result<T>
 * - Replace database::result<void> with common::VoidResult
 * - Replace database::error with common::error_info
 * - Replace is_error() with is_err()
 * - Replace has_value() with is_ok()
 * - Replace get_error() with error()
 *
 * @see https://github.com/kcenon/common_system/issues/205
 */

// Include common_system's error handling (Mandatory)
#include <kcenon/common/patterns/result.h>

namespace database {

// =============================================================================
// Primary types - USE THESE
// =============================================================================

/// @brief Primary Result type - use this for all new code
template<typename T>
using CommonResult = kcenon::common::Result<T>;

/// @brief Primary VoidResult type - use this for all new code
using CommonVoidResult = kcenon::common::VoidResult;

/// @brief Primary error type - use this for all new code
using CommonError = kcenon::common::error_info;

// =============================================================================
// Database-specific error codes
// =============================================================================

	// Error codes for database operations
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

	// Alias for common_system's error_info (use CommonError for new code)
	using error = kcenon::common::error_info;

// =============================================================================
// DEPRECATED - Legacy compatibility types (will be removed in future versions)
// =============================================================================

	/**
	 * @brief Legacy error_info struct for backward compatibility
	 * @deprecated Use common::error_info (or database::CommonError) instead
	 */
	struct [[deprecated("Use common::error_info instead")]] error_info {
		int code;
		std::string message;
		std::string module;

		error_info(int c = 0, std::string msg = "", std::string mod = "")
			: code(c), message(std::move(msg)), module(std::move(mod)) {}

		explicit error_info(const std::string& msg)
			: code(-1), message(msg), module("") {}

		// Conversion to/from common_system::error_info
		error_info(const error& e)
			: code(e.code), message(e.message), module(e.module) {}

		operator error() const {
			return kcenon::common::error_info(code, message, module);
		}
	};

	/**
	 * @brief Legacy result wrapper for backward compatibility
	 * @deprecated Use common::Result<T> (or database::CommonResult<T>) instead
	 *
	 * This wrapper class is deprecated. Please migrate to common::Result<T>.
	 * The wrapper provides legacy API methods (is_error, has_value, get_error)
	 * that are not available in common::Result<T>.
	 *
	 * Migration:
	 * - is_error() -> is_err()
	 * - has_value() -> is_ok()
	 * - get_error() -> error()
	 */
	template<typename T>
	class [[deprecated("Use common::Result<T> instead")]] result : public kcenon::common::Result<T> {
	public:
		using base_type = kcenon::common::Result<T>;
		using value_type = T;

		// Inherit all constructors
		using base_type::base_type;

		// Constructor from base type
		result(const base_type& other) : base_type(other) {}
		result(base_type&& other) : base_type(std::move(other)) {}

		// Constructor from error_info (for legacy compatibility)
		result(const error_info& e) : base_type(kcenon::common::error_info(e.code, e.message, e.module)) {}

		// Compatibility methods
		bool is_ok() const noexcept { return base_type::is_ok(); }
		bool is_err() const noexcept { return base_type::is_err(); }
		bool is_error() const noexcept { return base_type::is_err(); }
		bool has_value() const noexcept { return base_type::is_ok(); }

		// Error access
		const database::error& get_error() const { return base_type::error(); }
		database::error& get_error() {
			// const_cast is needed because error() is const
			return const_cast<database::error&>(base_type::error());
		}

		// Static factory methods for compatibility
		template<typename U = T>
		static result<T> ok(U&& value) {
			return result<T>(std::forward<U>(value));
		}

		static result<T> err(const database::error& e) {
			return result<T>(e);
		}

		static result<T> err(database::error&& e) {
			return result<T>(std::move(e));
		}
	};

	/**
	 * @brief Legacy result<void> specialization for backward compatibility
	 * @deprecated Use common::VoidResult (or database::CommonVoidResult) instead
	 */
	template<>
	class [[deprecated("Use common::VoidResult instead")]] result<void> : public kcenon::common::VoidResult {
	public:
		using base_type = kcenon::common::VoidResult;
		using value_type = void;

		// Default constructor for success
		result() : base_type(base_type::ok(std::monostate{})) {}

		// Constructor from base type
		result(const base_type& other) : base_type(other) {}
		result(base_type&& other) : base_type(std::move(other)) {}

		// Constructor from error (for error case)
		result(const kcenon::common::error_info& e) : base_type(e) {}
		result(kcenon::common::error_info&& e) : base_type(std::move(e)) {}

		// Constructor from database::error_info (for legacy compatibility)
		result(const error_info& e) : base_type(kcenon::common::error_info(e.code, e.message, e.module)) {}

		// Compatibility methods
		bool is_ok() const noexcept { return base_type::is_ok(); }
		bool is_err() const noexcept { return base_type::is_err(); }
		bool is_error() const noexcept { return base_type::is_err(); }
		bool has_value() const noexcept { return base_type::is_ok(); }

		// Error access
		const database::error& get_error() const { return base_type::error(); }
		database::error& get_error() {
			// const_cast is needed because error() is const
			return const_cast<database::error&>(base_type::error());
		}

		// Static factory methods for compatibility
		static result<void> ok(std::monostate = std::monostate{}) {
			return result<void>();
		}

		static result<void> err(const database::error& e) {
			return result<void>(e);
		}

		static result<void> err(database::error&& e) {
			return result<void>(std::move(e));
		}
	};

	// Legacy compatibility aliases
	// Note: [[deprecated]] attribute on using declarations requires C++14+
	template<typename T>
	using Result [[deprecated("Use common::Result<T> instead")]] = result<T>;

	using VoidResult [[deprecated("Use common::VoidResult instead")]] = kcenon::common::VoidResult;

} // namespace database

// =============================================================================
// Integrated namespace for compatibility
// =============================================================================

// Provide the same types in integrated namespace for compatibility
namespace database::integrated {
	// Deprecated legacy types
	using database::result;
	using database::error_code;
	using database::error;
	using database::error_info;
	using database::Result;
	using database::VoidResult;

	// Recommended types (from common_system)
	using database::CommonResult;
	using database::CommonVoidResult;
	using database::CommonError;
} // namespace database::integrated
