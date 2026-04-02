// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file database_manager.h
 * @brief Public forwarding header for the database manager.
 *
 * Exposes database::database_manager, the primary high-level interface
 * for database connections and query execution.
 *
 * @code
 * #include <kcenon/database/database_manager.h>
 *
 * auto context = std::make_shared<database::database_context>();
 * database::database_manager mgr(context);
 * mgr.set_mode(database::database_types::postgres);
 * @endcode
 */

#pragma once

#include "database/database_manager.h"
