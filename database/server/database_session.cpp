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

    if (!db_connection_) {
        response.success = false;
        response.error_message = "No database connection available";
        response.error_code = -2;
        return response;
    }

    try {
        // Execute query based on operation type
        switch (request.operation) {
            case protocol::query_operation::SELECT: {
                auto result = db_connection_->select_query(request.query_string);

                response.success = true;
                response.affected_rows = result.size();

                // Convert database_result to protocol format
                if (!result.empty()) {
                    // Extract column names from first row
                    for (const auto& [key, value] : result[0]) {
                        response.column_names.push_back(key);
                    }

                    // Convert rows
                    for (const auto& db_row : result) {
                        std::map<std::string, std::string> proto_row;
                        for (const auto& [key, value] : db_row) {
                            // Convert database_value to string
                            if (std::holds_alternative<std::string>(value)) {
                                proto_row[key] = std::get<std::string>(value);
                            } else if (std::holds_alternative<int64_t>(value)) {
                                proto_row[key] = std::to_string(std::get<int64_t>(value));
                            } else if (std::holds_alternative<double>(value)) {
                                proto_row[key] = std::to_string(std::get<double>(value));
                            } else if (std::holds_alternative<bool>(value)) {
                                proto_row[key] = std::get<bool>(value) ? "true" : "false";
                            } else {
                                proto_row[key] = "NULL";
                            }
                        }
                        response.rows.push_back(std::move(proto_row));
                    }
                }
                break;
            }

            case protocol::query_operation::INSERT: {
                unsigned int affected = db_connection_->insert_query(request.query_string);
                response.success = true;
                response.affected_rows = affected;
                break;
            }

            case protocol::query_operation::UPDATE: {
                unsigned int affected = db_connection_->update_query(request.query_string);
                response.success = true;
                response.affected_rows = affected;
                break;
            }

            case protocol::query_operation::DELETE: {
                unsigned int affected = db_connection_->delete_query(request.query_string);
                response.success = true;
                response.affected_rows = affected;
                break;
            }

            case protocol::query_operation::CREATE:
            case protocol::query_operation::ALTER:
            case protocol::query_operation::DROP:
            case protocol::query_operation::OTHER:
            default: {
                bool success = db_connection_->execute_query(request.query_string);
                response.success = success;
                if (!success) {
                    response.error_message = "Query execution failed";
                    response.error_code = -3;
                }
                break;
            }
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Exception during query execution: ") + e.what();
        response.error_code = -99;
    }

    return response;
}

protocol::transaction_response database_session::begin_transaction() {
    protocol::transaction_response response;

    if (in_transaction_.exchange(true)) {
        response.success = false;
        response.error_message = "Transaction already in progress";
        return response;
    }

    if (!db_connection_) {
        in_transaction_ = false;
        response.success = false;
        response.error_message = "No database connection available";
        return response;
    }

    try {
        bool success = db_connection_->execute_query("BEGIN TRANSACTION");
        if (!success) {
            in_transaction_ = false;
            response.success = false;
            response.error_message = "Failed to begin transaction";
        } else {
            response.success = true;
        }
    } catch (const std::exception& e) {
        in_transaction_ = false;
        response.success = false;
        response.error_message = std::string("Exception during begin transaction: ") + e.what();
    }

    return response;
}

protocol::transaction_response database_session::commit_transaction() {
    protocol::transaction_response response;

    if (!in_transaction_.exchange(false)) {
        response.success = false;
        response.error_message = "No active transaction";
        return response;
    }

    if (!db_connection_) {
        response.success = false;
        response.error_message = "No database connection available";
        return response;
    }

    try {
        bool success = db_connection_->execute_query("COMMIT");
        if (!success) {
            response.success = false;
            response.error_message = "Failed to commit transaction";
        } else {
            response.success = true;
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Exception during commit: ") + e.what();
    }

    return response;
}

protocol::transaction_response database_session::rollback_transaction() {
    protocol::transaction_response response;

    if (!in_transaction_.exchange(false)) {
        response.success = false;
        response.error_message = "No active transaction";
        return response;
    }

    if (!db_connection_) {
        response.success = false;
        response.error_message = "No database connection available";
        return response;
    }

    try {
        bool success = db_connection_->execute_query("ROLLBACK");
        if (!success) {
            response.success = false;
            response.error_message = "Failed to rollback transaction";
        } else {
            response.success = true;
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Exception during rollback: ") + e.what();
    }

    return response;
}

void database_session::close() {
    if (active_.exchange(false)) {
        // Rollback any active transaction
        if (in_transaction_.load()) {
            rollback_transaction();
        }

        // Close database connection
        if (db_connection_) {
            try {
                db_connection_->disconnect();
            } catch (const std::exception& e) {
                // Log error but don't throw from destructor path
                // In production, this would use proper logging
            }
            db_connection_.reset();
        }
    }
}

} // namespace database::server
