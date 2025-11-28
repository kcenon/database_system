/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#pragma once

#include <string>
#include <chrono>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>

namespace database::gateway {

/**
 * @brief Audit log entry structure
 */
struct audit_entry {
    std::chrono::system_clock::time_point timestamp;
    std::string user;
    std::string session_id;
    std::string client_ip;
    std::string operation;      // SELECT, INSERT, UPDATE, DELETE, etc.
    std::string query_hash;     // Hash of the query for grouping
    std::string target_cluster;
    bool success;
    std::chrono::milliseconds latency;
    std::string error_message;  // Only if success == false

    /**
     * @brief Convert entry to JSON string
     * @return JSON representation of the entry
     */
    std::string to_json() const;

    /**
     * @brief Convert entry to CSV line
     * @return CSV representation of the entry
     */
    std::string to_csv() const;
};

/**
 * @brief Audit log output format
 */
enum class audit_format {
    JSON,
    CSV
};

/**
 * @brief Audit logger configuration
 */
struct audit_logger_config {
    std::string log_path;                           // Base path for log files
    audit_format format{audit_format::JSON};        // Output format
    size_t max_file_size_mb{100};                   // Max file size before rotation
    size_t max_files{10};                           // Max number of rotated files
    bool async_write{true};                         // Use async writing
    size_t buffer_size{1000};                       // Max entries in write buffer
    std::chrono::milliseconds flush_interval{1000}; // Flush interval for async mode
};

/**
 * @class audit_logger
 * @brief File-based audit logger for database queries
 *
 * Features:
 * - JSON/CSV output formats
 * - File rotation based on size
 * - Asynchronous writing for performance
 * - Thread-safe operation
 * - Automatic buffer flushing
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses internal mutex and condition variable for synchronization
 *
 * Example Usage:
 * @code
 *   audit_logger_config config;
 *   config.log_path = "/var/log/db-gateway/audit.log";
 *   config.format = audit_format::JSON;
 *   config.max_file_size_mb = 100;
 *   config.async_write = true;
 *
 *   auto logger = std::make_unique<audit_logger>(config);
 *   logger->start();
 *
 *   audit_entry entry;
 *   entry.timestamp = std::chrono::system_clock::now();
 *   entry.user = "admin";
 *   entry.operation = "SELECT";
 *   entry.success = true;
 *   entry.latency = std::chrono::milliseconds(50);
 *
 *   logger->log(entry);
 *
 *   logger->stop();
 * @endcode
 */
class audit_logger {
public:
    /**
     * @brief Construct audit logger with configuration
     * @param config Logger configuration
     */
    explicit audit_logger(const audit_logger_config& config);

    /**
     * @brief Destructor - ensures proper cleanup and final flush
     */
    ~audit_logger();

    // Non-copyable, non-movable
    audit_logger(const audit_logger&) = delete;
    audit_logger& operator=(const audit_logger&) = delete;
    audit_logger(audit_logger&&) = delete;
    audit_logger& operator=(audit_logger&&) = delete;

    /**
     * @brief Start the audit logger
     * @return true on success, false on failure
     *
     * Opens log file and starts async writer thread if configured.
     */
    bool start();

    /**
     * @brief Stop the audit logger
     *
     * Flushes remaining entries and closes log file.
     */
    void stop();

    /**
     * @brief Check if logger is running
     * @return true if running
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Log an audit entry
     * @param entry Audit entry to log
     *
     * In async mode, entry is queued for background writing.
     * In sync mode, entry is written immediately.
     */
    void log(const audit_entry& entry);

    /**
     * @brief Flush pending entries to disk
     *
     * Blocks until all pending entries are written.
     */
    void flush();

    /**
     * @brief Get current log file path
     * @return Current log file path
     */
    std::string current_log_path() const;

    /**
     * @brief Get statistics
     * @return Map of statistics (entries_logged, entries_dropped, etc.)
     */
    std::map<std::string, uint64_t> get_stats() const;

private:
    /**
     * @brief Open or rotate log file
     * @return true on success
     */
    bool open_log_file();

    /**
     * @brief Check if file rotation is needed
     * @return true if rotation needed
     */
    bool needs_rotation() const;

    /**
     * @brief Rotate log file
     */
    void rotate_log_file();

    /**
     * @brief Write entry to file
     * @param entry Entry to write
     */
    void write_entry(const audit_entry& entry);

    /**
     * @brief Async writer thread function
     */
    void async_writer_loop();

    /**
     * @brief Generate rotated file name
     * @param index Rotation index
     * @return Rotated file name
     */
    std::string get_rotated_filename(size_t index) const;

    /**
     * @brief Get current file size in bytes
     * @return File size
     */
    size_t get_file_size() const;

    // Configuration
    audit_logger_config config_;

    // File handling
    std::ofstream log_file_;
    std::string current_path_;
    mutable std::mutex file_mutex_;

    // Async writing
    std::atomic<bool> running_{false};
    std::thread writer_thread_;
    std::queue<audit_entry> write_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Statistics
    std::atomic<uint64_t> entries_logged_{0};
    std::atomic<uint64_t> entries_dropped_{0};
    std::atomic<uint64_t> rotations_{0};
};

} // namespace database::gateway
