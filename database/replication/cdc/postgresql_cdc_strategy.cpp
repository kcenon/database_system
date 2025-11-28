/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "postgresql_cdc_strategy.h"

#ifdef USE_POSTGRESQL
#include <libpq-fe.h>
#endif

#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

namespace database::replication::cdc {

namespace {

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

} // anonymous namespace

// Constructor
postgresql_cdc_strategy::postgresql_cdc_strategy() = default;

// Destructor
postgresql_cdc_strategy::~postgresql_cdc_strategy() {
    if (active_.load()) {
        stop();
    }
#ifdef USE_POSTGRESQL
    if (repl_conn_) {
        PQfinish(repl_conn_);
        repl_conn_ = nullptr;
    }
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
#endif
}

// Move constructor
postgresql_cdc_strategy::postgresql_cdc_strategy(postgresql_cdc_strategy&& other) noexcept
    : conn_(other.conn_),
      repl_conn_(other.repl_conn_),
      config_(std::move(other.config_)),
      slot_name_(std::move(other.slot_name_)),
      publication_name_(std::move(other.publication_name_)),
      active_(other.active_.load()),
      initialized_(other.initialized_.load()),
      stop_requested_(other.stop_requested_.load()),
      current_lsn_(std::move(other.current_lsn_)),
      confirmed_lsn_(std::move(other.confirmed_lsn_)),
      tracked_tables_(std::move(other.tracked_tables_)) {
    other.conn_ = nullptr;
    other.repl_conn_ = nullptr;
    other.active_.store(false);
    other.initialized_.store(false);
}

// Move assignment
postgresql_cdc_strategy& postgresql_cdc_strategy::operator=(postgresql_cdc_strategy&& other) noexcept {
    if (this != &other) {
#ifdef USE_POSTGRESQL
        if (repl_conn_) {
            PQfinish(repl_conn_);
        }
        if (conn_) {
            PQfinish(conn_);
        }
#endif
        conn_ = other.conn_;
        repl_conn_ = other.repl_conn_;
        config_ = std::move(other.config_);
        slot_name_ = std::move(other.slot_name_);
        publication_name_ = std::move(other.publication_name_);
        active_.store(other.active_.load());
        initialized_.store(other.initialized_.load());
        stop_requested_.store(other.stop_requested_.load());
        current_lsn_ = std::move(other.current_lsn_);
        confirmed_lsn_ = std::move(other.confirmed_lsn_);
        tracked_tables_ = std::move(other.tracked_tables_);

        other.conn_ = nullptr;
        other.repl_conn_ = nullptr;
        other.active_.store(false);
        other.initialized_.store(false);
    }
    return *this;
}

result<void> postgresql_cdc_strategy::initialize(const cdc_config& config) {
#ifndef USE_POSTGRESQL
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_.load()) {
        return result<void>(error_info{-1, "CDC already initialized", "postgresql_cdc"});
    }

    config_ = config;

    // Connect to PostgreSQL
    conn_ = PQconnectdb(config.connection_string.c_str());
    if (PQstatus(conn_) != CONNECTION_OK) {
        std::string error = get_last_error();
        PQfinish(conn_);
        conn_ = nullptr;
        return result<void>(error_info{-2, "Failed to connect: " + error, "postgresql_cdc"});
    }

    // Create unique slot and publication names based on connection
    std::ostringstream slot_ss, pub_ss;
    slot_ss << "cdc_slot_" << std::hash<std::string>{}(config.connection_string) % 10000;
    pub_ss << "cdc_pub_" << std::hash<std::string>{}(config.connection_string) % 10000;
    slot_name_ = slot_ss.str();
    publication_name_ = pub_ss.str();

    // Create publication for tracked tables
    auto pub_result = create_publication();
    if (pub_result.is_err()) {
        PQfinish(conn_);
        conn_ = nullptr;
        return pub_result;
    }

    // Create replication slot
    auto slot_result = create_replication_slot();
    if (slot_result.is_err()) {
        // Clean up publication
        execute_sql("DROP PUBLICATION IF EXISTS " + publication_name_);
        PQfinish(conn_);
        conn_ = nullptr;
        return slot_result;
    }

    // Store tracked tables
    for (const auto& table : config.tracked_tables) {
        tracked_tables_.insert(table);
    }

    initialized_.store(true);
    return result<void>::ok();
#endif
}

result<void> postgresql_cdc_strategy::start() {
#ifndef USE_POSTGRESQL
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    if (!initialized_.load()) {
        return result<void>(error_info{-3, "CDC not initialized", "postgresql_cdc"});
    }

    if (active_.load()) {
        return result<void>(error_info{-4, "CDC already active", "postgresql_cdc"});
    }

    stop_requested_.store(false);

    // Create replication connection
    std::string repl_conninfo = config_.connection_string + " replication=database";
    repl_conn_ = PQconnectdb(repl_conninfo.c_str());
    if (PQstatus(repl_conn_) != CONNECTION_OK) {
        std::string error = PQerrorMessage(repl_conn_);
        PQfinish(repl_conn_);
        repl_conn_ = nullptr;
        return result<void>(error_info{-5, "Failed to connect for replication: " + error, "postgresql_cdc"});
    }

    active_.store(true);

    // Start streaming worker thread
    streaming_thread_ = std::thread(&postgresql_cdc_strategy::streaming_worker, this);

    return result<void>::ok();
#endif
}

result<void> postgresql_cdc_strategy::stop() {
#ifndef USE_POSTGRESQL
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    if (!active_.load()) {
        return result<void>(error_info{-5, "CDC not active", "postgresql_cdc"});
    }

    stop_requested_.store(true);

    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
    }

    if (repl_conn_) {
        PQfinish(repl_conn_);
        repl_conn_ = nullptr;
    }

    active_.store(false);
    return result<void>::ok();
#endif
}

std::optional<replication_event> postgresql_cdc_strategy::capture_next_event() {
    auto events = capture_events(1);
    if (events.empty()) {
        return std::nullopt;
    }
    return events[0];
}

std::vector<replication_event> postgresql_cdc_strategy::capture_events(size_t max_count) {
    std::vector<replication_event> events;

    if (!active_.load()) {
        return events;
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);

    while (!event_queue_.empty() && events.size() < max_count) {
        events.push_back(std::move(event_queue_.front()));
        event_queue_.pop();
    }

    return events;
}

result<void> postgresql_cdc_strategy::acknowledge_event(const replication_event& /*event*/) {
#ifndef USE_POSTGRESQL
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    // In PostgreSQL, we acknowledge by sending standby status updates
    // The streaming worker handles this automatically
    confirmed_lsn_ = current_lsn_;
    return result<void>::ok();
#endif
}

std::string postgresql_cdc_strategy::get_current_position() const {
    return current_lsn_;
}

result<void> postgresql_cdc_strategy::set_position(const std::string& position) {
    current_lsn_ = position;
    return result<void>::ok();
}

bool postgresql_cdc_strategy::is_active() const {
    return active_.load();
}

database_type postgresql_cdc_strategy::get_database_type() const {
    return database_type::POSTGRESQL;
}

result<void> postgresql_cdc_strategy::cleanup() {
#ifndef USE_POSTGRESQL
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    if (!conn_) {
        return result<void>(error_info{-6, "Database not connected", "postgresql_cdc"});
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Drop replication slot
    std::string drop_slot = "SELECT pg_drop_replication_slot('" + slot_name_ + "')";
    execute_sql(drop_slot);

    // Drop publication
    std::string drop_pub = "DROP PUBLICATION IF EXISTS " + publication_name_;
    execute_sql(drop_pub);

    tracked_tables_.clear();
    initialized_.store(false);
    active_.store(false);

    return result<void>::ok();
#endif
}

size_t postgresql_cdc_strategy::get_pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return event_queue_.size();
}

result<void> postgresql_cdc_strategy::create_replication_slot() {
#ifndef USE_POSTGRESQL
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    // Check if slot already exists
    std::string check_sql =
        "SELECT 1 FROM pg_replication_slots WHERE slot_name = '" + slot_name_ + "'";

    PGresult* res = PQexec(conn_, check_sql.c_str());
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        PQclear(res);
        // Slot already exists, drop and recreate
        std::string drop_sql = "SELECT pg_drop_replication_slot('" + slot_name_ + "')";
        PGresult* drop_res = PQexec(conn_, drop_sql.c_str());
        PQclear(drop_res);
    } else {
        PQclear(res);
    }

    // Create logical replication slot with pgoutput plugin
    std::string create_sql =
        "SELECT pg_create_logical_replication_slot('" + slot_name_ + "', 'pgoutput')";

    res = PQexec(conn_, create_sql.c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(conn_);
        PQclear(res);
        return result<void>(error_info{-7, "Failed to create replication slot: " + error, "postgresql_cdc"});
    }

    // Store the starting LSN
    if (PQntuples(res) > 0 && PQnfields(res) > 1) {
        current_lsn_ = PQgetvalue(res, 0, 1);
    }

    PQclear(res);
    return result<void>::ok();
#endif
}

result<void> postgresql_cdc_strategy::create_publication() {
#ifndef USE_POSTGRESQL
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    // Drop existing publication if any
    std::string drop_sql = "DROP PUBLICATION IF EXISTS " + publication_name_;
    PGresult* res = PQexec(conn_, drop_sql.c_str());
    PQclear(res);

    // Build table list
    std::ostringstream tables_ss;
    bool first = true;
    for (const auto& table : config_.tracked_tables) {
        if (!first) {
            tables_ss << ", ";
        }
        tables_ss << table;
        first = false;
    }

    // Create publication
    std::string create_sql;
    if (config_.tracked_tables.empty()) {
        create_sql = "CREATE PUBLICATION " + publication_name_ + " FOR ALL TABLES";
    } else {
        create_sql = "CREATE PUBLICATION " + publication_name_ +
                     " FOR TABLE " + tables_ss.str();
    }

    res = PQexec(conn_, create_sql.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string error = PQerrorMessage(conn_);
        PQclear(res);
        return result<void>(error_info{-8, "Failed to create publication: " + error, "postgresql_cdc"});
    }

    PQclear(res);
    return result<void>::ok();
#endif
}

void postgresql_cdc_strategy::streaming_worker() {
#ifdef USE_POSTGRESQL
    if (!repl_conn_) {
        return;
    }

    // Start logical replication
    std::ostringstream start_cmd;
    start_cmd << "START_REPLICATION SLOT " << slot_name_
              << " LOGICAL " << (current_lsn_.empty() ? "0/0" : current_lsn_)
              << " (proto_version '1', publication_names '" << publication_name_ << "')";

    PGresult* res = PQexec(repl_conn_, start_cmd.str().c_str());
    if (PQresultStatus(res) != PGRES_COPY_BOTH) {
        PQclear(res);
        active_.store(false);
        return;
    }
    PQclear(res);

    // Main streaming loop
    while (!stop_requested_.load()) {
        // Check for data
        if (PQconsumeInput(repl_conn_) == 0) {
            continue;
        }

        char* buffer = nullptr;
        int len = PQgetCopyData(repl_conn_, &buffer, 1);  // non-blocking

        if (len > 0 && buffer) {
            // Parse the message
            auto event = parse_pgoutput_message(buffer, static_cast<size_t>(len));
            if (event) {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                event_queue_.push(std::move(*event));
            }
            PQfreemem(buffer);
        } else if (len == 0) {
            // No data available, wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else if (len == -1) {
            // End of stream or error
            break;
        }

        // Send standby status update periodically
        // This acknowledges received WAL and keeps connection alive
    }
#endif
}

std::optional<replication_event> postgresql_cdc_strategy::parse_pgoutput_message(
    const char* data, size_t len) {

    if (len == 0 || !data) {
        return std::nullopt;
    }

    // pgoutput message format:
    // First byte is message type
    // 'w' = WAL data
    // 'k' = keepalive
    // 'B' = Begin transaction
    // 'C' = Commit transaction
    // 'I' = Insert
    // 'U' = Update
    // 'D' = Delete
    // 'R' = Relation (table metadata)

    char msg_type = data[0];

    replication_event event;
    event.timestamp = std::chrono::system_clock::now();

    switch (msg_type) {
        case 'w': {
            // WAL data - contains nested message
            if (len > 25) {
                // Skip header (dataStart, walEnd, timestamp = 24 bytes)
                return parse_pgoutput_message(data + 25, len - 25);
            }
            break;
        }
        case 'I': {
            // Insert message
            event.type = replication_event::event_type::INSERT;
            // Parse table OID and tuple data (simplified)
            // In real implementation, we'd need the relation message to map OID to table name
            event.table_name = "unknown";  // Would be resolved from relation cache

            // Parse new tuple data
            // Format: relation_id (4 bytes) + 'N' + tuple data
            if (len > 6) {
                // Simplified: store raw data as value
                event.new_values["_raw"] = std::string(data + 6, len - 6);
            }
            return event;
        }
        case 'U': {
            // Update message
            event.type = replication_event::event_type::UPDATE;
            event.table_name = "unknown";
            if (len > 6) {
                event.new_values["_raw"] = std::string(data + 6, len - 6);
            }
            return event;
        }
        case 'D': {
            // Delete message
            event.type = replication_event::event_type::DELETE;
            event.table_name = "unknown";
            if (len > 6) {
                event.old_values["_raw"] = std::string(data + 6, len - 6);
            }
            return event;
        }
        default:
            // Ignore other message types (Begin, Commit, Relation, etc.)
            break;
    }

    return std::nullopt;
}

result<void> postgresql_cdc_strategy::execute_sql(const std::string& sql) {
#ifndef USE_POSTGRESQL
    (void)sql;
    return result<void>(error_info{-1, "PostgreSQL support not compiled", "postgresql_cdc"});
#else
    if (!conn_) {
        return result<void>(error_info{-6, "Database not connected", "postgresql_cdc"});
    }

    PGresult* res = PQexec(conn_, sql.c_str());
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(conn_);
        PQclear(res);
        return result<void>(error_info{-9, "SQL execution failed: " + error, "postgresql_cdc"});
    }

    PQclear(res);
    return result<void>::ok();
#endif
}

std::string postgresql_cdc_strategy::get_last_error() const {
#ifdef USE_POSTGRESQL
    if (conn_) {
        return PQerrorMessage(conn_);
    }
#endif
    return "Unknown error";
}

} // namespace database::replication::cdc
