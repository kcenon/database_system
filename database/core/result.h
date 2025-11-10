#pragma once

// Result<T> header for database_system
// This file provides a fallback Result<T> implementation when BUILD_WITH_COMMON_SYSTEM is not enabled

#ifdef BUILD_WITH_COMMON_SYSTEM
	// Use common_system's Result implementation
	#include <kcenon/common/patterns/result.h>
	namespace database {
		using kcenon::common::Result;
		using kcenon::common::VoidResult;
		using kcenon::common::error_info;
	}
	namespace database::integrated {
		using kcenon::common::Result;
		using kcenon::common::VoidResult;
		using kcenon::common::error_info;
	}
#else
	// Fallback: Define minimal Result<T> type if common_system not available
	#include <variant>
	#include <string>
	#include <utility>

	namespace database {
		/// Error information structure
		struct error_info {
			int code;
			std::string message;
			std::string module;

			// Default constructor
			error_info(int c = 0, std::string msg = "", std::string mod = "")
				: code(c), message(std::move(msg)), module(std::move(mod)) {}

			// Constructor accepting message only (for compatibility with common_system)
			explicit error_info(const std::string& msg)
				: code(-1), message(msg), module("") {}
		};

		/// Result type for database operations
		template<typename T>
		class Result {
		private:
			std::variant<T, error_info> value_;

		public:
			// Constructors
			Result(T&& value) : value_(std::forward<T>(value)) {}
			Result(const T& value) : value_(value) {}
			Result(error_info&& error) : value_(std::forward<error_info>(error)) {}
			Result(const error_info& error) : value_(error) {}

			// Static factory methods (for compatibility with common_system)
			template<typename U = T>
			static Result<T> ok(U&& value) {
				return Result<T>(std::forward<U>(value));
			}

			static Result<T> err(const error_info& error) {
				return Result<T>(error);
			}

			static Result<T> err(error_info&& error) {
				return Result<T>(std::move(error));
			}

			// Check status
			bool is_ok() const { return std::holds_alternative<T>(value_); }
			bool is_err() const { return std::holds_alternative<error_info>(value_); }
			bool is_error() const { return is_err(); }

			// Access value
			const T& value() const { return std::get<T>(value_); }
			T& value() { return std::get<T>(value_); }

			// Access error
			const error_info& error() const { return std::get<error_info>(value_); }
			error_info& error() { return std::get<error_info>(value_); }

			// Bool conversion
			explicit operator bool() const { return is_ok(); }
		};

		/// VoidResult for operations that don't return a value
		using VoidResult = Result<std::monostate>;
	}

	// Provide the same types in integrated namespace for compatibility
	namespace database::integrated {
		using database::error_info;
		using database::Result;
		using database::VoidResult;
	}
#endif
