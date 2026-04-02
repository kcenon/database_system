// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_builder.h
 * @brief Public forwarding header for the universal query builder.
 *
 * Exposes database::query_builder, database::query_condition, and
 * related enums (join_type, sort_order) for constructing database
 * queries across different backends.
 *
 * @code
 * #include <kcenon/database/query_builder.h>
 *
 * database::query_builder builder(database::database_types::postgres);
 * auto query = builder.select({"id", "name"})
 *     .from("users")
 *     .where("active", "=", true)
 *     .build();
 * @endcode
 */

#pragma once

#include "database/query_builder.h"
