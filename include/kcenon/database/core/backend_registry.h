// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file backend_registry.h
 * @brief Public forwarding header for database backend registry.
 *
 * Exposes database::core::backend_registry for runtime backend
 * registration and creation.
 *
 * @code
 * #include <kcenon/database/core/backend_registry.h>
 *
 * auto backend = database::core::create_backend("postgresql");
 * @endcode
 */

#pragma once

#include "database/core/backend_registry.h"
