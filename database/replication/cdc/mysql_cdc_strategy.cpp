/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "mysql_cdc_strategy.h"

#ifdef USE_MYSQL
#include <mysql/mysql.h>
#endif

#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <random>
#include <regex>

namespace database::replication::cdc {

namespace {

/**
 * @brief MySQL binlog event types
 */
enum mysql_binlog_event_type {
    UNKNOWN_EVENT = 0,
    QUERY_EVENT = 2,
    ROTATE_EVENT = 4,
    TABLE_MAP_EVENT = 19,
    WRITE_ROWS_EVENT_V1 = 23,
    UPDATE_ROWS_EVENT_V1 = 24,
    DELETE_ROWS_EVENT_V1 = 25,
    WRITE_ROWS_EVENT_V2 = 30,
    UPDATE_ROWS_EVENT_V2 = 31,
    DELETE_ROWS_EVENT_V2 = 32,
    GTID_EVENT = 33
};

/**
 * @brief Generate random server ID for replication
 */
uint32_t generate_server_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(1000000, 9999999);
    return dis(gen);
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
mysql_cdc_strategy::mysql_cdc_strategy()
    : server_id_(generate_server_id()) {
}

// Destructor
mysql_cdc_strategy::~mysql_cdc_strategy() {
    if (active_.load()) {
        stop();
    }
#ifdef USE_MYSQL
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }
#endif
}

// Move constructor
mysql_cdc_strategy::mysql_cdc_strategy(mysql_cdc_strategy&& other) noexcept
    : conn_(other.conn_),
      config_(std::move(other.config_)),
      host_(std::move(other.host_)),
      port_(other.port_),
      user_(std::move(other.user_)),
      password_(std::move(other.password_)),
      database_(std::move(other.database_)),
      binlog_file_(std::move(other.binlog_file_)),
      binlog_position_(other.binlog_position_),
      gtid_set_(std::move(other.gtid_set_)),
      use_gtid_(other.use_gtid_),
      server_id_(other.server_id_),
      active_(other.active_.load()),
      initialized_(other.initialized_.load()),
      stop_requested_(other.stop_requested_.load()),
      tracked_tables_(std::move(other.tracked_tables_)),
      table_map_(std::move(other.table_map_)) {
    other.conn_ = nullptr;
    other.active_.store(false);
    other.initialized_.store(false);
}

// Move assignment
mysql_cdc_strategy& mysql_cdc_strategy::operator=(mysql_cdc_strategy&& other) noexcept {
    if (this != &other) {
#ifdef USE_MYSQL
        if (conn_) {
            mysql_close(conn_);
        }
#endif
        conn_ = other.conn_;
        config_ = std::move(other.config_);
        host_ = std::move(other.host_);
        port_ = other.port_;
        user_ = std::move(other.user_);
        password_ = std::move(other.password_);
        database_ = std::move(other.database_);
        binlog_file_ = std::move(other.binlog_file_);
        binlog_position_ = other.binlog_position_;
        gtid_set_ = std::move(other.gtid_set_);
        use_gtid_ = other.use_gtid_;
        server_id_ = other.server_id_;
        active_.store(other.active_.load());
        initialized_.store(other.initialized_.load());
        stop_requested_.store(other.stop_requested_.load());
        tracked_tables_ = std::move(other.tracked_tables_);
        table_map_ = std::move(other.table_map_);

        other.conn_ = nullptr;
        other.active_.store(false);
        other.initialized_.store(false);
    }
    return *this;
}

void mysql_cdc_strategy::parse_connection_string() {
    // Parse mysql://user:password@host:port/database format
    std::regex url_regex(R"(mysql://(?:([^:]+):([^@]+)@)?([^:\/]+)(?::(\d+))?(?:\/(.+))?)");
    std::smatch matches;

    if (std::regex_match(config_.connection_string, matches, url_regex)) {
        if (matches[1].matched) user_ = matches[1].str();
        if (matches[2].matched) password_ = matches[2].str();
        if (matches[3].matched) host_ = matches[3].str();
        if (matches[4].matched) port_ = std::stoi(matches[4].str());
        if (matches[5].matched) database_ = matches[5].str();
    } else {
        // Assume it's host:port format or just host
        host_ = config_.connection_string;
    }

    if (host_.empty()) {
        host_ = "localhost";
    }
}

result<void> mysql_cdc_strategy::initialize(const cdc_config& config) {
#ifndef USE_MYSQL
    (void)config;
    return result<void>(error_info{-1, "MySQL support not compiled", "mysql_cdc"});
#else
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_.load()) {
        return result<void>(error_info{-1, "CDC already initialized", "mysql_cdc"});
    }

    config_ = config;
    parse_connection_string();

    // Initialize MySQL client library
    conn_ = mysql_init(nullptr);
    if (!conn_) {
        return result<void>(error_info{-2, "Failed to initialize MySQL client", "mysql_cdc"});
    }

    // Connect to MySQL
    if (!mysql_real_connect(conn_, host_.c_str(), user_.c_str(), password_.c_str(),
                            database_.empty() ? nullptr : database_.c_str(),
                            port_, nullptr, 0)) {
        std::string error = get_last_error();
        mysql_close(conn_);
        conn_ = nullptr;
        return result<void>(error_info{-3, "Failed to connect: " + error, "mysql_cdc"});
    }

    // Check binlog format
    auto format_result = execute_sql("SELECT @@binlog_format");
    if (format_result.is_err()) {
        mysql_close(conn_);
        conn_ = nullptr;
        return result<void>(error_info{-4, "Cannot verify binlog format", "mysql_cdc"});
    }

    // Get current binlog position
    auto pos_result = fetch_current_binlog_position();
    if (pos_result.is_err()) {
        mysql_close(conn_);
        conn_ = nullptr;
        return pos_result;
    }

    // Store tracked tables
    for (const auto& table : config.tracked_tables) {
        tracked_tables_.insert(table);
    }

    initialized_.store(true);
    return result<void>::ok();
#endif
}

result<void> mysql_cdc_strategy::fetch_current_binlog_position() {
#ifndef USE_MYSQL
    return result<void>(error_info{-1, "MySQL support not compiled", "mysql_cdc"});
#else
    // Try GTID first
    if (mysql_query(conn_, "SELECT @@gtid_mode") == 0) {
        MYSQL_RES* res = mysql_store_result(conn_);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0] && std::string(row[0]) == "ON") {
                use_gtid_ = true;
            }
            mysql_free_result(res);
        }
    }

    if (use_gtid_) {
        // Get executed GTID set
        if (mysql_query(conn_, "SELECT @@gtid_executed") == 0) {
            MYSQL_RES* res = mysql_store_result(conn_);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) {
                    gtid_set_ = row[0];
                }
                mysql_free_result(res);
            }
        }
    }

    // Get binlog file and position
    if (mysql_query(conn_, "SHOW MASTER STATUS") == 0) {
        MYSQL_RES* res = mysql_store_result(conn_);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                if (row[0]) binlog_file_ = row[0];
                if (row[1]) binlog_position_ = std::stoull(row[1]);
            }
            mysql_free_result(res);
        }
    }

    if (binlog_file_.empty()) {
        return result<void>(error_info{-5, "Cannot get binlog position", "mysql_cdc"});
    }

    return result<void>::ok();
#endif
}

result<void> mysql_cdc_strategy::start() {
#ifndef USE_MYSQL
    return result<void>(error_info{-1, "MySQL support not compiled", "mysql_cdc"});
#else
    if (!initialized_.load()) {
        return result<void>(error_info{-6, "CDC not initialized", "mysql_cdc"});
    }

    if (active_.load()) {
        return result<void>(error_info{-7, "CDC already active", "mysql_cdc"});
    }

    stop_requested_.store(false);
    active_.store(true);

    // Start binlog streaming worker thread
    binlog_thread_ = std::thread(&mysql_cdc_strategy::binlog_worker, this);

    return result<void>::ok();
#endif
}

result<void> mysql_cdc_strategy::stop() {
#ifndef USE_MYSQL
    return result<void>(error_info{-1, "MySQL support not compiled", "mysql_cdc"});
#else
    if (!active_.load()) {
        return result<void>(error_info{-8, "CDC not active", "mysql_cdc"});
    }

    stop_requested_.store(true);

    if (binlog_thread_.joinable()) {
        binlog_thread_.join();
    }

    active_.store(false);
    return result<void>::ok();
#endif
}

std::optional<replication_event> mysql_cdc_strategy::capture_next_event() {
    auto events = capture_events(1);
    if (events.empty()) {
        return std::nullopt;
    }
    return events[0];
}

std::vector<replication_event> mysql_cdc_strategy::capture_events(size_t max_count) {
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

result<void> mysql_cdc_strategy::acknowledge_event(const replication_event& /*event*/) {
    // Position is tracked automatically during streaming
    return result<void>::ok();
}

std::string mysql_cdc_strategy::get_current_position() const {
    if (use_gtid_ && !gtid_set_.empty()) {
        return "gtid:" + gtid_set_;
    }
    return binlog_file_ + ":" + std::to_string(binlog_position_);
}

result<void> mysql_cdc_strategy::set_position(const std::string& position) {
    if (position.substr(0, 5) == "gtid:") {
        use_gtid_ = true;
        gtid_set_ = position.substr(5);
    } else {
        auto colon_pos = position.find(':');
        if (colon_pos != std::string::npos) {
            binlog_file_ = position.substr(0, colon_pos);
            binlog_position_ = std::stoull(position.substr(colon_pos + 1));
        }
    }
    return result<void>::ok();
}

bool mysql_cdc_strategy::is_active() const {
    return active_.load();
}

database_type mysql_cdc_strategy::get_database_type() const {
    return database_type::MYSQL;
}

result<void> mysql_cdc_strategy::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    tracked_tables_.clear();
    table_map_.clear();
    initialized_.store(false);
    active_.store(false);

    return result<void>::ok();
}

size_t mysql_cdc_strategy::get_pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return event_queue_.size();
}

void mysql_cdc_strategy::binlog_worker() {
#ifdef USE_MYSQL
    // Create a new connection for binlog reading
    MYSQL* binlog_conn = mysql_init(nullptr);
    if (!binlog_conn) {
        active_.store(false);
        return;
    }

    if (!mysql_real_connect(binlog_conn, host_.c_str(), user_.c_str(), password_.c_str(),
                            nullptr, port_, nullptr, 0)) {
        mysql_close(binlog_conn);
        active_.store(false);
        return;
    }

    // Set up binlog dump request
    std::ostringstream cmd;
    cmd << "SET @master_binlog_checksum = @@global.binlog_checksum";
    mysql_query(binlog_conn, cmd.str().c_str());

    // Request binlog events using COM_BINLOG_DUMP
    // This is a simplified version - real implementation would use mysql_binlog_open
    cmd.str("");
    cmd << "SHOW BINLOG EVENTS IN '" << binlog_file_ << "' FROM " << binlog_position_;

    while (!stop_requested_.load()) {
        if (mysql_query(binlog_conn, cmd.str().c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(binlog_conn);
            if (res) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) && !stop_requested_.load()) {
                    // Columns: Log_name, Pos, Event_type, Server_id, End_log_pos, Info
                    if (row[2]) {
                        std::string event_type = row[2];
                        std::string table_name;
                        std::string info = row[5] ? row[5] : "";

                        // Parse based on event type
                        if (event_type == "Write_rows" || event_type == "Write_rows_v1") {
                            replication_event event;
                            event.type = replication_event::event_type::INSERT;
                            event.timestamp = std::chrono::system_clock::now();
                            event.table_name = info;
                            event.new_values["_info"] = info;

                            if (tracked_tables_.empty() ||
                                tracked_tables_.count(event.table_name) > 0) {
                                std::lock_guard<std::mutex> lock(queue_mutex_);
                                event_queue_.push(std::move(event));
                            }
                        } else if (event_type == "Update_rows" || event_type == "Update_rows_v1") {
                            replication_event event;
                            event.type = replication_event::event_type::UPDATE;
                            event.timestamp = std::chrono::system_clock::now();
                            event.table_name = info;
                            event.new_values["_info"] = info;

                            if (tracked_tables_.empty() ||
                                tracked_tables_.count(event.table_name) > 0) {
                                std::lock_guard<std::mutex> lock(queue_mutex_);
                                event_queue_.push(std::move(event));
                            }
                        } else if (event_type == "Delete_rows" || event_type == "Delete_rows_v1") {
                            replication_event event;
                            event.type = replication_event::event_type::DELETE;
                            event.timestamp = std::chrono::system_clock::now();
                            event.table_name = info;
                            event.old_values["_info"] = info;

                            if (tracked_tables_.empty() ||
                                tracked_tables_.count(event.table_name) > 0) {
                                std::lock_guard<std::mutex> lock(queue_mutex_);
                                event_queue_.push(std::move(event));
                            }
                        }

                        // Update position
                        if (row[4]) {
                            binlog_position_ = std::stoull(row[4]);
                        }
                    }
                }
                mysql_free_result(res);
            }
        }

        // Wait before next poll
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Update query for next position
        cmd.str("");
        cmd << "SHOW BINLOG EVENTS IN '" << binlog_file_ << "' FROM " << binlog_position_;
    }

    mysql_close(binlog_conn);
#endif
}

replication_event mysql_cdc_strategy::parse_row_event(
    int event_type,
    const std::string& table_name,
    const std::string& row_data) {

    replication_event event;
    event.table_name = table_name;
    event.timestamp = std::chrono::system_clock::now();

    switch (event_type) {
        case WRITE_ROWS_EVENT_V1:
        case WRITE_ROWS_EVENT_V2:
            event.type = replication_event::event_type::INSERT;
            event.new_values["_raw"] = row_data;
            break;
        case UPDATE_ROWS_EVENT_V1:
        case UPDATE_ROWS_EVENT_V2:
            event.type = replication_event::event_type::UPDATE;
            event.new_values["_raw"] = row_data;
            break;
        case DELETE_ROWS_EVENT_V1:
        case DELETE_ROWS_EVENT_V2:
            event.type = replication_event::event_type::DELETE;
            event.old_values["_raw"] = row_data;
            break;
        default:
            event.type = replication_event::event_type::INSERT;
            break;
    }

    return event;
}

result<void> mysql_cdc_strategy::execute_sql(const std::string& sql) {
#ifndef USE_MYSQL
    (void)sql;
    return result<void>(error_info{-1, "MySQL support not compiled", "mysql_cdc"});
#else
    if (!conn_) {
        return result<void>(error_info{-9, "Database not connected", "mysql_cdc"});
    }

    if (mysql_query(conn_, sql.c_str()) != 0) {
        return result<void>(error_info{-10, "SQL execution failed: " + get_last_error(), "mysql_cdc"});
    }

    // Consume result if any
    MYSQL_RES* res = mysql_store_result(conn_);
    if (res) {
        mysql_free_result(res);
    }

    return result<void>::ok();
#endif
}

std::string mysql_cdc_strategy::get_last_error() const {
#ifdef USE_MYSQL
    if (conn_) {
        return mysql_error(conn_);
    }
#endif
    return "Unknown error";
}

} // namespace database::replication::cdc
