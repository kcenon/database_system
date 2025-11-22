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

// Result<T> header for database_system
// Provides unified error handling via common_system integration

// Include common_system's error handling (Mandatory)
#include <kcenon/common/patterns/result.h>

namespace database {
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

	// Primary type aliases using common_system's result
	using error = kcenon::common::error_info;

	// Compatibility type for error_info (legacy, will be deprecated)
	// Must be defined before result<T> classes
	struct error_info {
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

	// Compatibility layer: wrapper around common_system::Result with legacy API
	template<typename T>
	class result : public kcenon::common::Result<T> {
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

	// Specialization for void
	template<>
	class result<void> : public kcenon::common::VoidResult {
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

	// Legacy compatibility aliases (will be deprecated)
	// Note: [[deprecated]] attribute on using declarations requires C++20
	// For C++17 compatibility, we omit the attribute here
	template<typename T>
	using Result = result<T>;

	using VoidResult = result<void>;

} // namespace database

// Provide the same types in integrated namespace for compatibility
namespace database::integrated {
	using database::result;
	using database::error_code;
	using database::error;
	using database::error_info;

	// Legacy compatibility
	using database::Result;
	using database::VoidResult;
} // namespace database::integrated
