// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file database_context.h
 * @brief Public forwarding header for database dependency injection context.
 *
 * Exposes database::database_context for creating database_manager
 * instances with dependency injection.
 *
 * @code
 * #include <kcenon/database/core/database_context.h>
 *
 * auto context = std::make_shared<database::database_context>();
 * @endcode
 */

#pragma once

#include "database/core/database_context.h"
