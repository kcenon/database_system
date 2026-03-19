// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file database_backend.h
 * @brief Public forwarding header for database backend interface.
 *
 * Exposes database::core::database_backend abstract base class and
 * associated types (database_value, database_row, database_result,
 * connection_config, backend_factory_fn).
 *
 * @code
 * #include <kcenon/database/core/database_backend.h>
 *
 * class my_backend : public database::core::database_backend {
 *     // ...
 * };
 * @endcode
 */

#pragma once

#include "database/core/database_backend.h"
