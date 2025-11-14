/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "database_session.h"

namespace database::server {

database_session::database_session(std::string session_id, std::shared_ptr<database_base> db_connection)
    : session_id_(std::move(session_id))
    , db_connection_(std::move(db_connection))
    , active_(true)
    , in_transaction_(false)
{
}

database_session::~database_session() {
    close();
}

protocol::query_response database_session::execute_query(const protocol::query_request& request) {
    protocol::query_response response;

    if (!active_.load()) {
        response.success = false;
        response.error_message = "Session is closed";
        response.error_code = -1;
        return response;
    }

    // TODO: Implement actual query execution
    // For now, return stub response
    response.success = true;
    response.affected_rows = 0;
    response.error_message = "TODO: Implement query execution";

    return response;
}

protocol::transaction_response database_session::begin_transaction() {
    protocol::transaction_response response;

    if (in_transaction_.exchange(true)) {
        response.success = false;
        response.error_message = "Transaction already in progress";
        return response;
    }

    // TODO: Implement actual transaction begin
    response.success = true;
    return response;
}

protocol::transaction_response database_session::commit_transaction() {
    protocol::transaction_response response;

    if (!in_transaction_.exchange(false)) {
        response.success = false;
        response.error_message = "No active transaction";
        return response;
    }

    // TODO: Implement actual transaction commit
    response.success = true;
    return response;
}

protocol::transaction_response database_session::rollback_transaction() {
    protocol::transaction_response response;

    if (!in_transaction_.exchange(false)) {
        response.success = false;
        response.error_message = "No active transaction";
        return response;
    }

    // TODO: Implement actual transaction rollback
    response.success = true;
    return response;
}

void database_session::close() {
    if (active_.exchange(false)) {
        // Rollback any active transaction
        if (in_transaction_.load()) {
            rollback_transaction();
        }
        // TODO: Close database connection
    }
}

} // namespace database::server
