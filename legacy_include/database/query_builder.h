#pragma once

// ===========================================================================
// DEPRECATED legacy include path.
//
// This forwarding header exists only for backward compatibility with consumers
// that include <database/query_builder.h>. It is DEPRECATED and scheduled for
// removal in v1.2.0.
//
// Migrate to the canonical include path:
//     #include <kcenon/database/query_builder.h>
//
// The canonical C++ namespace is `kcenon::database`. The unqualified
// `database` namespace remains available as a backward-compatibility alias.
// ===========================================================================

#include "kcenon/database/query_builder.h"
