/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "database/core/database_context.h"
#include "database/connection_pool.h"
#include "database/monitoring/performance_monitor.h"
#include "database/leak_detector_enhanced.h"
#include "database/orm/entity.h"
#include "database/async/async_operations.h"
#include "database/security/secure_connection.h"

namespace database
{

database_context::database_context()
    : pool_manager_(std::make_shared<connection_pool_manager>())
    , performance_monitor_(std::make_shared<monitoring::performance_monitor>())
    , leak_detector_(nullptr) // Will be created when needed with specific pool
    , entity_manager_(std::make_shared<orm::entity_manager>())
    , transaction_coordinator_(std::make_shared<async::transaction_coordinator>())
    , credential_manager_(std::make_shared<security::credential_manager>())
    , access_control_(std::make_shared<security::access_control>())
    , audit_logger_(std::make_shared<security::audit_logger>())
    , security_monitor_(std::make_shared<security::security_monitor>())
    , encryption_manager_(std::make_shared<security::encryption_manager>())
{
    // Sprint 2, Task 2.3: Initialize connection pool manager
    // Sprint 3, Task 3.2: Initialize performance monitor
    // Sprint 3, Task 3.1: Initialize ORM components
    // Sprint 3, Task 3.3: Initialize security components
    // Note: leak_detector requires a connection_pool, so it's created on-demand
}

database_context::~database_context()
{
    // Cleanup: All shared_ptrs will be automatically destroyed
    // Sprint 3, Task 3.2: performance_monitor and leak_detector cleanup
}

} // namespace database
