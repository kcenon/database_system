/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "audit_logger.h"

#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace database::gateway {

namespace {

/**
 * @brief Escape a string for JSON output
 */
std::string escape_json_string(const std::string& input) {
    std::string output;
    output.reserve(input.size() * 2);

    for (char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b";  break;
            case '\f': output += "\\f";  break;
            case '\n': output += "\\n";  break;
            case '\r': output += "\\r";  break;
            case '\t': output += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control character - escape as unicode
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setw(4)
                        << std::setfill('0') << static_cast<int>(c);
                    output += oss.str();
                } else {
                    output += c;
                }
                break;
        }
    }

    return output;
}

/**
 * @brief Escape a string for CSV output
 */
std::string escape_csv_string(const std::string& input) {
    bool needs_quotes = false;

    for (char c : input) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return input;
    }

    std::string output = "\"";
    for (char c : input) {
        if (c == '"') {
            output += "\"\"";  // Double quotes
        } else {
            output += c;
        }
    }
    output += "\"";

    return output;
}

/**
 * @brief Format timestamp as ISO 8601
 */
std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()
    ).count() % 1000;

    std::tm tm_val{};
#ifdef _WIN32
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms << 'Z';

    return oss.str();
}

} // anonymous namespace

// audit_entry implementation
std::string audit_entry::to_json() const {
    std::ostringstream oss;

    oss << "{";
    oss << "\"timestamp\":\"" << format_timestamp(timestamp) << "\"";
    oss << ",\"user\":\"" << escape_json_string(user) << "\"";
    oss << ",\"session_id\":\"" << escape_json_string(session_id) << "\"";
    oss << ",\"client_ip\":\"" << escape_json_string(client_ip) << "\"";
    oss << ",\"operation\":\"" << escape_json_string(operation) << "\"";
    oss << ",\"query_hash\":\"" << escape_json_string(query_hash) << "\"";
    oss << ",\"target_cluster\":\"" << escape_json_string(target_cluster) << "\"";
    oss << ",\"success\":" << (success ? "true" : "false");
    oss << ",\"latency_ms\":" << latency.count();

    if (!success && !error_message.empty()) {
        oss << ",\"error\":\"" << escape_json_string(error_message) << "\"";
    }

    oss << "}";

    return oss.str();
}

std::string audit_entry::to_csv() const {
    std::ostringstream oss;

    oss << format_timestamp(timestamp) << ",";
    oss << escape_csv_string(user) << ",";
    oss << escape_csv_string(session_id) << ",";
    oss << escape_csv_string(client_ip) << ",";
    oss << escape_csv_string(operation) << ",";
    oss << escape_csv_string(query_hash) << ",";
    oss << escape_csv_string(target_cluster) << ",";
    oss << (success ? "true" : "false") << ",";
    oss << latency.count() << ",";
    oss << escape_csv_string(error_message);

    return oss.str();
}

// audit_logger implementation
audit_logger::audit_logger(const audit_logger_config& config)
    : config_(config)
{
}

audit_logger::~audit_logger() {
    stop();
}

bool audit_logger::start() {
    if (running_.load()) {
        return true;  // Already running
    }

    if (!open_log_file()) {
        return false;
    }

    running_.store(true);

    if (config_.async_write) {
        writer_thread_ = std::thread(&audit_logger::async_writer_loop, this);
    }

    return true;
}

void audit_logger::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    // Wake up writer thread
    queue_cv_.notify_all();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }

    // Final flush
    flush();

    std::lock_guard<std::mutex> lock(file_mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void audit_logger::log(const audit_entry& entry) {
    if (!running_.load()) {
        return;
    }

    if (config_.async_write) {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        if (write_queue_.size() >= config_.buffer_size) {
            // Buffer full - drop oldest entry
            entries_dropped_.fetch_add(1);
            write_queue_.pop();
        }

        write_queue_.push(entry);
        queue_cv_.notify_one();
    } else {
        write_entry(entry);
    }
}

void audit_logger::flush() {
    if (config_.async_write) {
        // Process all queued entries
        std::queue<audit_entry> entries_to_write;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            std::swap(entries_to_write, write_queue_);
        }

        while (!entries_to_write.empty()) {
            write_entry(entries_to_write.front());
            entries_to_write.pop();
        }
    }

    std::lock_guard<std::mutex> lock(file_mutex_);
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}

std::string audit_logger::current_log_path() const {
    std::lock_guard<std::mutex> lock(file_mutex_);
    return current_path_;
}

std::map<std::string, uint64_t> audit_logger::get_stats() const {
    return {
        {"entries_logged", entries_logged_.load()},
        {"entries_dropped", entries_dropped_.load()},
        {"rotations", rotations_.load()},
        {"queue_size", [this]() {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            return static_cast<uint64_t>(write_queue_.size());
        }()}
    };
}

bool audit_logger::open_log_file() {
    std::lock_guard<std::mutex> lock(file_mutex_);

    current_path_ = config_.log_path;

    // Create directory if it doesn't exist
    std::filesystem::path log_path(current_path_);
    auto parent_dir = log_path.parent_path();

    if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
        try {
            std::filesystem::create_directories(parent_dir);
        } catch (const std::filesystem::filesystem_error&) {
            return false;
        }
    }

    // Open file in append mode
    log_file_.open(current_path_, std::ios::app);

    if (!log_file_.is_open()) {
        return false;
    }

    // Write header for CSV format
    if (config_.format == audit_format::CSV && get_file_size() == 0) {
        log_file_ << "timestamp,user,session_id,client_ip,operation,"
                  << "query_hash,target_cluster,success,latency_ms,error\n";
    }

    return true;
}

bool audit_logger::needs_rotation() const {
    if (config_.max_file_size_mb == 0) {
        return false;  // Rotation disabled
    }

    size_t max_bytes = config_.max_file_size_mb * 1024 * 1024;
    return get_file_size() >= max_bytes;
}

void audit_logger::rotate_log_file() {
    std::lock_guard<std::mutex> lock(file_mutex_);

    if (log_file_.is_open()) {
        log_file_.close();
    }

    // Shift existing rotated files
    for (size_t i = config_.max_files - 1; i > 0; --i) {
        std::string old_name = get_rotated_filename(i - 1);
        std::string new_name = get_rotated_filename(i);

        if (std::filesystem::exists(old_name)) {
            try {
                if (std::filesystem::exists(new_name)) {
                    std::filesystem::remove(new_name);
                }
                std::filesystem::rename(old_name, new_name);
            } catch (const std::filesystem::filesystem_error&) {
                // Ignore rotation errors
            }
        }
    }

    // Rename current log to .1
    if (std::filesystem::exists(config_.log_path)) {
        try {
            std::string first_rotated = get_rotated_filename(1);
            if (std::filesystem::exists(first_rotated)) {
                std::filesystem::remove(first_rotated);
            }
            std::filesystem::rename(config_.log_path, first_rotated);
        } catch (const std::filesystem::filesystem_error&) {
            // Ignore
        }
    }

    // Open new log file
    log_file_.open(config_.log_path, std::ios::app);

    // Write header for CSV format
    if (config_.format == audit_format::CSV) {
        log_file_ << "timestamp,user,session_id,client_ip,operation,"
                  << "query_hash,target_cluster,success,latency_ms,error\n";
    }

    rotations_.fetch_add(1);
}

void audit_logger::write_entry(const audit_entry& entry) {
    // Check rotation before writing
    if (needs_rotation()) {
        rotate_log_file();
    }

    std::string line;
    if (config_.format == audit_format::JSON) {
        line = entry.to_json() + "\n";
    } else {
        line = entry.to_csv() + "\n";
    }

    {
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_ << line;
            entries_logged_.fetch_add(1);
        }
    }
}

void audit_logger::async_writer_loop() {
    while (running_.load()) {
        std::queue<audit_entry> entries_to_write;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Wait for entries or flush interval
            queue_cv_.wait_for(lock, config_.flush_interval, [this]() {
                return !write_queue_.empty() || !running_.load();
            });

            if (!running_.load() && write_queue_.empty()) {
                break;
            }

            // Move entries to local queue
            std::swap(entries_to_write, write_queue_);
        }

        // Write entries outside the lock
        while (!entries_to_write.empty()) {
            write_entry(entries_to_write.front());
            entries_to_write.pop();
        }

        // Flush to disk
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (log_file_.is_open()) {
            log_file_.flush();
        }
    }
}

std::string audit_logger::get_rotated_filename(size_t index) const {
    if (index == 0) {
        return config_.log_path;
    }

    std::filesystem::path log_path(config_.log_path);
    std::string stem = log_path.stem().string();
    std::string ext = log_path.extension().string();
    auto parent = log_path.parent_path();

    std::ostringstream oss;
    oss << stem << "." << index << ext;

    return (parent / oss.str()).string();
}

size_t audit_logger::get_file_size() const {
    // Note: file_mutex_ should already be held by caller
    if (!log_file_.is_open()) {
        return 0;
    }

    try {
        return std::filesystem::file_size(current_path_);
    } catch (const std::filesystem::filesystem_error&) {
        return 0;
    }
}

} // namespace database::gateway
