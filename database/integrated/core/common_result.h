// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

/**
 * @file common_result.h
 * @brief Common Result<T> pattern for integrated database system
 *
 * This file provides compatibility with existing code that expects common:: namespace.
 */

#pragma once

#if defined(BUILD_WITH_COMMON_SYSTEM) || defined(USE_COMMON_SYSTEM)
	// Use common_system's Result implementation directly
	#include <kcenon/common/patterns/result.h>

	// Re-export at global common:: namespace for backward compatibility
	namespace common {
		template<typename T>
		using Result = kcenon::common::Result<T>;
		using VoidResult = kcenon::common::VoidResult;
		using error_info = kcenon::common::error_info;

		// Add ok() helper for VoidResult
		inline VoidResult ok() {
			return VoidResult(std::monostate{});
		}
	}

	// Import monitoring interfaces separately (only when monitoring_interface.h is included)
	#ifdef KCENON_COMMON_INTERFACES_MONITORING_INTERFACE_H
	namespace common {
		namespace interfaces = kcenon::common::interfaces;
	}
	#endif
#else
	// Fallback: minimal Result<T> implementation
	#include <variant>
	#include <string>
	#include <utility>

	namespace common {
		struct error_info {
			int code{0};
			std::string message;
			std::string context;
		};

		template<typename T>
		class Result {
		public:
			Result(T value) : data_(std::move(value)) {}
			Result(error_info error) : data_(std::move(error)) {}

			bool is_ok() const { return std::holds_alternative<T>(data_); }
			bool is_error() const { return !is_ok(); }

			T& value() { return std::get<T>(data_); }
			const T& value() const { return std::get<T>(data_); }

			error_info& error() { return std::get<error_info>(data_); }
			const error_info& error() const { return std::get<error_info>(data_); }

		private:
			std::variant<T, error_info> data_;
		};

		using VoidResult = Result<std::monostate>;
	}
#endif
