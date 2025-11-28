/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "sqlite_cdc_strategy.h"

#include <sqlite3.h>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace database::replication::cdc {

namespace {

/**
 * @brief Convert operation string to event type
 */
replication_event::event_type string_to_event_type(const std::string& op) {
    if (op == "INSERT") {
        return replication_event::event_type::INSERT;
    } else if (op == "UPDATE") {
        return replication_event::event_type::UPDATE;
    } else {
        return replication_event::event_type::DELETE;
    }
}

/**
 * @brief Parse ISO 8601 timestamp string to time_point
 */
std::chrono::system_clock::time_point parse_timestamp(const std::string& ts) {
    std::tm tm = {};
    std::istringstream ss(ts);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

/**
 * @brief Get current timestamp as ISO 8601 string
 */
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t_now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief Escape single quotes in SQL string
 */
std::string escape_sql_string(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}

/**
 * @brief Simple JSON value escaping
 */
std::string escape_json_string(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

} // anonymous namespace

// Constructor
sqlite_cdc_strategy::sqlite_cdc_strategy() = default;

// Destructor
sqlite_cdc_strategy::~sqlite_cdc_strategy() {
    if (active_.load()) {
        stop();
    }
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

// Move constructor
sqlite_cdc_strategy::sqlite_cdc_strategy(sqlite_cdc_strategy&& other) noexcept
    : db_(other.db_),
      config_(std::move(other.config_)),
      active_(other.active_.load()),
      initialized_(other.initialized_.load()),
      last_processed_id_(other.last_processed_id_),
      tracked_tables_(std::move(other.tracked_tables_)) {
    other.db_ = nullptr;
    other.active_.store(false);
    other.initialized_.store(false);
}

// Move assignment
sqlite_cdc_strategy& sqlite_cdc_strategy::operator=(sqlite_cdc_strategy&& other) noexcept {
    if (this != &other) {
        if (db_) {
            sqlite3_close(db_);
        }
        db_ = other.db_;
        config_ = std::move(other.config_);
        active_.store(other.active_.load());
        initialized_.store(other.initialized_.load());
        last_processed_id_ = other.last_processed_id_;
        tracked_tables_ = std::move(other.tracked_tables_);

        other.db_ = nullptr;
        other.active_.store(false);
        other.initialized_.store(false);
    }
    return *this;
}

result<void> sqlite_cdc_strategy::initialize(const cdc_config& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_.load()) {
        return result<void>(error_info{-1, "CDC already initialized", "sqlite_cdc"});
    }

    config_ = config;

    // Open SQLite database
    int rc = sqlite3_open(config.connection_string.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string error = db_ ? sqlite3_errmsg(db_) : "Failed to allocate memory";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return result<void>(error_info{-2, "Failed to open database: " + error, "sqlite_cdc"});
    }

    // Enable foreign keys and WAL mode for better performance
    execute_sql("PRAGMA foreign_keys = ON");
    execute_sql("PRAGMA journal_mode = WAL");

    // Create change tables and triggers for each tracked table
    for (const auto& table : config.tracked_tables) {
        auto result = create_change_table(table);
        if (result.is_err()) {
            return result;
        }

        result = create_triggers(table);
        if (result.is_err()) {
            return result;
        }

        tracked_tables_.insert(table);
    }

    initialized_.store(true);
    return result<void>::ok();
}

result<void> sqlite_cdc_strategy::start() {
    if (!initialized_.load()) {
        return result<void>(error_info{-3, "CDC not initialized", "sqlite_cdc"});
    }

    if (active_.load()) {
        return result<void>(error_info{-4, "CDC already active", "sqlite_cdc"});
    }

    active_.store(true);
    return result<void>::ok();
}

result<void> sqlite_cdc_strategy::stop() {
    if (!active_.load()) {
        return result<void>(error_info{-5, "CDC not active", "sqlite_cdc"});
    }

    active_.store(false);
    return result<void>::ok();
}

std::optional<replication_event> sqlite_cdc_strategy::capture_next_event() {
    auto events = capture_events(1);
    if (events.empty()) {
        return std::nullopt;
    }
    return events[0];
}

std::vector<replication_event> sqlite_cdc_strategy::capture_events(size_t max_count) {
    std::vector<replication_event> events;

    if (!active_.load() || !db_) {
        return events;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Query all change tables for unprocessed events
    for (const auto& table : tracked_tables_) {
        std::string change_table = config_.change_table_prefix + table + "_changes";

        std::ostringstream query;
        query << "SELECT id, operation, old_values, new_values, timestamp "
              << "FROM " << change_table << " "
              << "WHERE id > " << last_processed_id_ << " AND processed = 0 "
              << "ORDER BY id ASC "
              << "LIMIT " << (max_count - events.size());

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query.str().c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            continue;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW && events.size() < max_count) {
            int64_t id = sqlite3_column_int64(stmt, 0);
            const char* op = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* old_val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            const char* new_val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const char* ts = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

            events.push_back(parse_change_record(
                table,
                id,
                op ? op : "",
                old_val ? old_val : "",
                new_val ? new_val : "",
                ts ? ts : current_timestamp()
            ));
        }

        sqlite3_finalize(stmt);

        if (events.size() >= max_count) {
            break;
        }
    }

    return events;
}

result<void> sqlite_cdc_strategy::acknowledge_event(const replication_event& event) {
    if (!db_) {
        return result<void>(error_info{-6, "Database not connected", "sqlite_cdc"});
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Find the change ID from the event
    // We store the change ID in the event's table_name field as "table:id"
    // For now, we mark all events up to the current position as processed
    std::string change_table = config_.change_table_prefix + event.table_name + "_changes";

    std::ostringstream sql;
    sql << "UPDATE " << change_table << " SET processed = 1 WHERE id <= "
        << last_processed_id_;

    auto result = execute_sql(sql.str());
    if (result.is_err()) {
        return result;
    }

    return result<void>::ok();
}

std::string sqlite_cdc_strategy::get_current_position() const {
    return std::to_string(last_processed_id_);
}

result<void> sqlite_cdc_strategy::set_position(const std::string& position) {
    try {
        last_processed_id_ = std::stoll(position);
        return result<void>::ok();
    } catch (const std::exception& e) {
        return result<void>(error_info{-7, "Invalid position: " + std::string(e.what()), "sqlite_cdc"});
    }
}

bool sqlite_cdc_strategy::is_active() const {
    return active_.load();
}

database_type sqlite_cdc_strategy::get_database_type() const {
    return database_type::SQLITE;
}

result<void> sqlite_cdc_strategy::cleanup() {
    if (!db_) {
        return result<void>(error_info{-6, "Database not connected", "sqlite_cdc"});
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Drop triggers and change tables for each tracked table
    for (const auto& table : tracked_tables_) {
        // Drop triggers
        execute_sql("DROP TRIGGER IF EXISTS " + config_.change_table_prefix + table + "_insert");
        execute_sql("DROP TRIGGER IF EXISTS " + config_.change_table_prefix + table + "_update");
        execute_sql("DROP TRIGGER IF EXISTS " + config_.change_table_prefix + table + "_delete");

        // Drop change table
        execute_sql("DROP TABLE IF EXISTS " + config_.change_table_prefix + table + "_changes");
    }

    tracked_tables_.clear();
    initialized_.store(false);
    active_.store(false);

    return result<void>::ok();
}

size_t sqlite_cdc_strategy::get_pending_count() const {
    if (!db_ || !active_.load()) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;

    for (const auto& table : tracked_tables_) {
        std::string change_table = config_.change_table_prefix + table + "_changes";

        std::ostringstream query;
        query << "SELECT COUNT(*) FROM " << change_table
              << " WHERE id > " << last_processed_id_ << " AND processed = 0";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query.str().c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            count += static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    return count;
}

result<void> sqlite_cdc_strategy::create_change_table(const std::string& table_name) {
    std::string change_table = config_.change_table_prefix + table_name + "_changes";

    std::ostringstream sql;
    sql << "CREATE TABLE IF NOT EXISTS " << change_table << " ("
        << "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        << "operation TEXT NOT NULL, "
        << "old_values TEXT, "
        << "new_values TEXT, "
        << "timestamp TEXT NOT NULL DEFAULT (datetime('now', 'localtime')), "
        << "processed INTEGER NOT NULL DEFAULT 0"
        << ")";

    auto result = execute_sql(sql.str());
    if (result.is_err()) {
        return result;
    }

    // Create index for efficient querying
    std::ostringstream index_sql;
    index_sql << "CREATE INDEX IF NOT EXISTS idx_" << change_table << "_pending "
              << "ON " << change_table << "(processed, id)";

    return execute_sql(index_sql.str());
}

result<void> sqlite_cdc_strategy::create_triggers(const std::string& table_name) {
    std::string change_table = config_.change_table_prefix + table_name + "_changes";
    auto columns = get_table_columns(table_name);

    if (columns.empty()) {
        return result<void>(error_info{-8, "Table not found or has no columns: " + table_name, "sqlite_cdc"});
    }

    // Build JSON object construction for columns
    std::ostringstream new_json;
    std::ostringstream old_json;

    new_json << "'{' || ";
    old_json << "'{' || ";

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& col = columns[i];
        if (i > 0) {
            new_json << " || ',' || ";
            old_json << " || ',' || ";
        }
        new_json << "'\"" << col << "\":\"' || COALESCE(REPLACE(NEW." << col << ", '\"', '\\\"'), 'null') || '\"'";
        old_json << "'\"" << col << "\":\"' || COALESCE(REPLACE(OLD." << col << ", '\"', '\\\"'), 'null') || '\"'";
    }

    new_json << " || '}'";
    old_json << " || '}'";

    // INSERT trigger
    std::ostringstream insert_trigger;
    insert_trigger << "CREATE TRIGGER IF NOT EXISTS " << config_.change_table_prefix << table_name << "_insert "
                   << "AFTER INSERT ON " << table_name << " "
                   << "BEGIN "
                   << "INSERT INTO " << change_table << " (operation, new_values) "
                   << "VALUES ('INSERT', " << new_json.str() << "); "
                   << "END";

    auto result = execute_sql(insert_trigger.str());
    if (result.is_err()) {
        return result;
    }

    // UPDATE trigger
    std::ostringstream update_trigger;
    update_trigger << "CREATE TRIGGER IF NOT EXISTS " << config_.change_table_prefix << table_name << "_update "
                   << "AFTER UPDATE ON " << table_name << " "
                   << "BEGIN "
                   << "INSERT INTO " << change_table << " (operation, old_values, new_values) "
                   << "VALUES ('UPDATE', " << old_json.str() << ", " << new_json.str() << "); "
                   << "END";

    result = execute_sql(update_trigger.str());
    if (result.is_err()) {
        return result;
    }

    // DELETE trigger
    std::ostringstream delete_trigger;
    delete_trigger << "CREATE TRIGGER IF NOT EXISTS " << config_.change_table_prefix << table_name << "_delete "
                   << "AFTER DELETE ON " << table_name << " "
                   << "BEGIN "
                   << "INSERT INTO " << change_table << " (operation, old_values) "
                   << "VALUES ('DELETE', " << old_json.str() << "); "
                   << "END";

    return execute_sql(delete_trigger.str());
}

std::vector<std::string> sqlite_cdc_strategy::get_table_columns(const std::string& table_name) {
    std::vector<std::string> columns;

    if (!db_) {
        return columns;
    }

    std::string query = "PRAGMA table_info(" + table_name + ")";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return columns;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name) {
            columns.emplace_back(name);
        }
    }

    sqlite3_finalize(stmt);
    return columns;
}

replication_event sqlite_cdc_strategy::parse_change_record(
    const std::string& table_name,
    int64_t change_id,
    const std::string& operation,
    const std::string& old_values,
    const std::string& new_values,
    const std::string& timestamp
) {
    replication_event event;
    event.table_name = table_name;
    event.type = string_to_event_type(operation);
    event.timestamp = parse_timestamp(timestamp);
    event.old_values = parse_json_values(old_values);
    event.new_values = parse_json_values(new_values);

    // Update last processed ID
    if (change_id > last_processed_id_) {
        last_processed_id_ = change_id;
    }

    return event;
}

std::unordered_map<std::string, std::string> sqlite_cdc_strategy::parse_json_values(
    const std::string& json_str
) {
    std::unordered_map<std::string, std::string> result;

    if (json_str.empty() || json_str == "null") {
        return result;
    }

    // Simple JSON parsing (assumes format: {"key":"value","key2":"value2"})
    size_t pos = 0;
    while (pos < json_str.size()) {
        // Find key start
        size_t key_start = json_str.find('"', pos);
        if (key_start == std::string::npos) break;

        // Find key end
        size_t key_end = json_str.find('"', key_start + 1);
        if (key_end == std::string::npos) break;

        std::string key = json_str.substr(key_start + 1, key_end - key_start - 1);

        // Find value start
        size_t value_start = json_str.find('"', key_end + 1);
        if (value_start == std::string::npos) break;

        // Find value end (handle escaped quotes)
        size_t value_end = value_start + 1;
        while (value_end < json_str.size()) {
            if (json_str[value_end] == '"' && json_str[value_end - 1] != '\\') {
                break;
            }
            ++value_end;
        }

        std::string value = json_str.substr(value_start + 1, value_end - value_start - 1);

        // Unescape value
        std::string unescaped;
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '\\' && i + 1 < value.size()) {
                char next = value[i + 1];
                switch (next) {
                    case '"': unescaped += '"'; ++i; break;
                    case '\\': unescaped += '\\'; ++i; break;
                    case 'n': unescaped += '\n'; ++i; break;
                    case 'r': unescaped += '\r'; ++i; break;
                    case 't': unescaped += '\t'; ++i; break;
                    default: unescaped += value[i]; break;
                }
            } else {
                unescaped += value[i];
            }
        }

        result[key] = unescaped;
        pos = value_end + 1;
    }

    return result;
}

result<void> sqlite_cdc_strategy::execute_sql(const std::string& sql) {
    if (!db_) {
        return result<void>(error_info{-6, "Database not connected", "sqlite_cdc"});
    }

    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);

    if (rc != SQLITE_OK) {
        std::string error = error_msg ? error_msg : "Unknown error";
        sqlite3_free(error_msg);
        return result<void>(error_info{-9, "SQL execution failed: " + error, "sqlite_cdc"});
    }

    return result<void>::ok();
}

} // namespace database::replication::cdc
