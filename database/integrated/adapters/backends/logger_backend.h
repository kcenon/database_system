// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
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
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file logger_backend.h
 * @brief Abstract interface for logger backends
 *
 * Defines the interface that all logger backends must implement.
 * This enables runtime selection of logging implementation without
 * conditional compilation.
 */

#pragma once

#include "../../core/common_result.h"
#include "../../core/configuration.h"

#include <string>

namespace database
{
namespace integrated
{
namespace adapters
{
namespace backends
{

/**
 * @class logger_backend
 * @brief Abstract base class for logger backends
 *
 * All logger backends (system, fallback, null) must implement this interface.
 * This enables runtime polymorphism and eliminates conditional compilation.
 */
class logger_backend
{
public:
	virtual ~logger_backend() = default;

	/**
	 * @brief Initialize the logger backend
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult initialize() = 0;

	/**
	 * @brief Shutdown the logger backend gracefully
	 * @return VoidResult::ok() on success, error on failure
	 */
	virtual common::VoidResult shutdown() = 0;

	/**
	 * @brief Check if backend is initialized
	 * @return true if initialized and ready to log
	 */
	virtual bool is_initialized() const = 0;

	/**
	 * @brief Log a message
	 * @param level Log level
	 * @param message Message to log
	 */
	virtual void log(db_log_level level, const std::string& message) = 0;

	/**
	 * @brief Flush pending log messages
	 */
	virtual void flush() = 0;
};

} // namespace backends
} // namespace adapters
} // namespace integrated
} // namespace database
