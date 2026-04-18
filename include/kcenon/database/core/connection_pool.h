// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file connection_pool.h
 * @brief Public forwarding header for the database connection pool.
 *
 * Exposes database::core::pool::connection_pool and supporting types for
 * thread-safe, RAII-based connection pooling.
 *
 * @code
 * #include <kcenon/database/core/connection_pool.h>
 * #include <kcenon/database/core/backend_registry.h>
 *
 * using namespace database::core;
 *
 * pool::pool_config cfg;
 * cfg.max_size = 16;
 *
 * auto pool = pool::connection_pool::create(
 *     cfg,
 *     [config]() {
 *         auto backend = backend_registry::instance().create("postgresql");
 *         if (!backend) return std::unique_ptr<database_backend>{};
 *         auto init = backend->initialize(config);
 *         if (init.is_err()) return std::unique_ptr<database_backend>{};
 *         return backend;
 *     });
 *
 * auto lease = pool->acquire();
 * if (lease.is_ok()) {
 *     auto rows = lease.value()->select_query("SELECT 1");
 * }
 * @endcode
 */

#pragma once

#include "database/core/connection_pool.h"
