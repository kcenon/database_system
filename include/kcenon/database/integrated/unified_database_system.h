// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file unified_database_system.h
 * @brief Public forwarding header for the unified database system.
 *
 * Exposes database::integrated::unified_database_system and related
 * types (query_result, database_metrics, health_check, transaction, etc.)
 * for zero-configuration database access.
 *
 * @code
 * #include <kcenon/database/integrated/unified_database_system.h>
 *
 * database::integrated::unified_database_system db;
 * auto result = db.connect("postgresql://localhost/mydb");
 * @endcode
 */

#pragma once

#include "database/integrated/unified_database_system.h"
