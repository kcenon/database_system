// BSD 3-Clause License
// Copyright (c) 2021-2025
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file compat.h
 * @brief Backward-compatibility namespace alias for the legacy ::database namespace.
 *
 * The public include path is `kcenon/database/...` and, as of issue #591, the
 * internal namespace is `kcenon::database`. Prior to that migration the library
 * defined its symbols in the top-level `database` namespace, and external
 * consumers (as well as some in-tree adapters) reference them as `database::...`
 * or `::database::...`.
 *
 * This header declares a global-scope namespace alias
 *
 *     namespace database = kcenon::database;
 *
 * so that pre-#591 source keeps compiling without changes. The alias is a thin,
 * zero-cost compatibility shim — it introduces no new symbols, only a second
 * spelling for `kcenon::database`.
 *
 * @deprecated The `database::` spelling is deprecated. Migrate to
 *             `kcenon::database::`. The alias is scheduled for removal in
 *             version 2.0.0 (see CHANGELOG.md and docs/MIGRATION.md). This is
 *             the same lifecycle window as the `<database/...>` legacy include
 *             shims (issue #582), so consumers can migrate path and namespace
 *             together.
 *
 * Define `DATABASE_DISABLE_LEGACY_NAMESPACE` before including any database_system
 * header to compile without the alias and surface every remaining `database::`
 * reference at build time.
 *
 * @see https://github.com/kcenon/database_system/issues/591
 */

#ifndef DATABASE_DISABLE_LEGACY_NAMESPACE
// Ensure the alias target exists even when this header is included before any
// other database_system header has opened the namespace. A namespace alias
// requires its target to be declared.
namespace kcenon { namespace database {} }

/// @brief Deprecated alias for kcenon::database. Use kcenon::database directly.
namespace database = kcenon::database;
#endif
