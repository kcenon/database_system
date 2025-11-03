// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

/**
 * @file common_result.h
 * @brief Common Result<T> pattern for integrated database system
 *
 * Provides a fallback Result<T> implementation when common_system is not available.
 * This ensures the integrated database system can work without external dependencies.
 */

#pragma once

#if defined(USE_COMMON_SYSTEM)
	#include <kcenon/common/patterns/result.h>
#else
	#include <string>
	#include <utility>

namespace common
{
	/**
	 * @brief Error structure for Result<T> pattern
	 */
	struct Error
	{
		std::string message; ///< Error message
		int code;            ///< Error code
	};

	/**
	 * @brief Result<T> pattern for type-safe error handling
	 *
	 * @tparam T The value type on success
	 */
	template <typename T>
	class Result
	{
	public:
		/**
		 * @brief Construct a successful result
		 * @param value The success value
		 */
		Result(T value) : value_(std::move(value)), has_value_(true)
		{
		}

		/**
		 * @brief Construct an error result
		 * @param error The error information
		 */
		Result(Error error) : error_(std::move(error)), has_value_(false)
		{
		}

		/**
		 * @brief Check if result contains a value
		 * @return true if successful, false if error
		 */
		bool is_ok() const
		{
			return has_value_;
		}

		/**
		 * @brief Get the success value
		 * @return Reference to the value
		 * @note Only call if is_ok() returns true
		 */
		const T& value() const
		{
			return value_;
		}

		/**
		 * @brief Get the error information
		 * @return Reference to the error
		 * @note Only call if is_ok() returns false
		 */
		const Error& error() const
		{
			return error_;
		}

		/**
		 * @brief Conversion operator for use in boolean context
		 */
		explicit operator bool() const
		{
			return has_value_;
		}

	private:
		T value_;        ///< The success value
		Error error_;    ///< The error information
		bool has_value_; ///< True if contains value, false if error
	};

	/**
	 * @brief Type alias for void results (operations with no return value)
	 */
	using VoidResult = Result<bool>;

	/**
	 * @brief Helper function to create a successful void result
	 * @return A successful VoidResult
	 */
	inline VoidResult ok()
	{
		return VoidResult(true);
	}

	/**
	 * @brief Helper function to create an error void result
	 * @param message Error message
	 * @param code Error code
	 * @return An error VoidResult
	 */
	inline VoidResult error(const std::string& message, int code = -1)
	{
		return VoidResult(Error{message, code});
	}
} // namespace common
#endif
