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

// Result<T> header for database_system
// This file integrates thread_system's result<T> for unified error handling

// Include thread_system's error handling
#include <kcenon/thread/core/error_handling.h>
#include <variant>

namespace database {
	// Primary type aliases using thread_system's result
	using error_code = kcenon::thread::error_code;
	using error = kcenon::thread::error;

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

		// Conversion to/from thread_system::error
		error_info(const error& e)
			: code(static_cast<int>(e.code())), message(e.message()), module("") {}

		operator error() const {
			return kcenon::thread::error(static_cast<error_code>(code), message);
		}
	};

	// Compatibility layer: wrapper around thread_system::result with legacy API
	template<typename T>
	class result : public kcenon::thread::result<T> {
	public:
		using base_type = kcenon::thread::result<T>;
		using value_type = T;

		// Inherit all constructors
		using base_type::base_type;

		// Constructor from base type
		result(const base_type& other) : base_type(other) {}
		result(base_type&& other) : base_type(std::move(other)) {}

		// Constructor from error_info (for legacy compatibility)
		result(const error_info& e) : base_type(kcenon::thread::error(static_cast<error_code>(e.code), e.message)) {}

		// Compatibility methods
		bool is_ok() const noexcept { return this->has_value(); }
		bool is_err() const noexcept { return !this->has_value(); }
		bool is_error() const noexcept { return !this->has_value(); }

		// Accessing error with legacy method name
		const database::error& error() const { return this->get_error(); }
		database::error& error() { return this->get_error(); }

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
	class result<void> : public kcenon::thread::result<void> {
	public:
		using base_type = kcenon::thread::result<void>;
		using value_type = void;

		// Inherit constructors
		using base_type::base_type;

		// Default constructor for success
		result() : base_type() {}

		// Constructor from base type
		result(const base_type& other) : base_type(other) {}
		result(base_type&& other) : base_type(std::move(other)) {}

		// Constructor from std::monostate (for legacy compatibility)
		result(std::monostate) : base_type() {}

		// Constructor from error_info (for legacy compatibility)
		result(const error_info& e) : base_type(kcenon::thread::error(static_cast<error_code>(e.code), e.message)) {}

		// Compatibility methods
		bool is_ok() const noexcept { return this->has_value(); }
		bool is_err() const noexcept { return !this->has_value(); }
		bool is_error() const noexcept { return !this->has_value(); }

		// Accessing error with legacy method name
		const database::error& error() const { return this->get_error(); }
		database::error& error() { return this->get_error(); }

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
