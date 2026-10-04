// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/core/database_context.h>
#include <kcenon/database/monitoring/performance_monitor.h>
#include <kcenon/database/orm/entity.h>
#include <kcenon/database/async/async_operations.h>
#include <kcenon/database/security/secure_connection.h>

namespace kcenon::database
{

database_context::database_context()
    : performance_monitor_(std::make_shared<monitoring::performance_monitor>())
    , entity_manager_(std::make_shared<orm::entity_manager>())
    , transaction_coordinator_(std::make_shared<async::transaction_coordinator>())
    , credential_manager_(std::make_shared<security::credential_manager>())
    , access_control_(std::make_shared<security::access_control>())
    , audit_logger_(std::make_shared<security::audit_logger>())
    , security_monitor_(std::make_shared<security::security_monitor>())
    , encryption_manager_(std::make_shared<security::encryption_manager>())
{
    // Sprint 3, Task 3.2: Initialize performance monitor
    // Sprint 3, Task 3.1: Initialize ORM components
    // Sprint 3, Task 3.3: Initialize security components
}

database_context::~database_context()
{
    // Cleanup: All shared_ptrs will be automatically destroyed
}

} // namespace kcenon::database
