/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "replication_manager.h"
#include "safe_query_builder.h"
#include "cdc/cdc_factory.h"
#include "../core/backend_registry.h"

#include <algorithm>
#include <sstream>

namespace database::replication {

namespace {

/**
 * @brief Health check threshold for replication lag
 */
constexpr std::chrono::milliseconds HEALTH_LAG_THRESHOLD{5000};

/**
 * @brief Worker thread sleep interval
 */
constexpr std::chrono::milliseconds WORKER_SLEEP_INTERVAL{10};

/**
 * @brief Default batch interval for CDC capture
 */
constexpr std::chrono::milliseconds CDC_POLL_INTERVAL{50};

} // anonymous namespace

// Constructor
replication_manager::replication_manager() = default;

// Destructor
replication_manager::~replication_manager() {
    if (active_.load()) {
        stop_replication();
    }
}

result<void> replication_manager::start_replication(
    const distributed::node_config& source,
    const distributed::node_config& target,
    const replication_config& config
) {
    if (active_.load()) {
        return result<void>(error_info{-1, "Replication already active", "replication"});
    }

    if (logger_) {
        logger_->log(integrated::db_log_level::info,
            "Starting replication from " + source.id + " to " + target.id);
    }

    source_config_ = source;
    target_config_ = target;
    config_ = config;

    // Initialize source connection
    auto source_result = initialize_source();
    if (source_result.is_err()) {
        if (logger_) {
            logger_->log_error("initialize_source", source_result.error().message, "");
        }
        return source_result;
    }

    // Initialize target connection
    auto target_result = initialize_target();
    if (target_result.is_err()) {
        if (logger_) {
            logger_->log_error("initialize_target", target_result.error().message, "");
        }
        return target_result;
    }

    // Reset statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = replication_stats{};
        stats_.last_event_time = std::chrono::steady_clock::now();
    }

    // Start worker threads
    active_.store(true);
    paused_.store(false);

    cdc_thread_ = std::thread(&replication_manager::cdc_worker, this);
    replication_thread_ = std::thread(&replication_manager::replication_worker, this);

    if (logger_) {
        logger_->log(integrated::db_log_level::info,
            "Replication started successfully with " +
            std::to_string(config.tables.size()) + " table(s)");
    }

    return result<void>::ok();
}

result<void> replication_manager::stop_replication() {
    if (!active_.load()) {
        return result<void>(error_info{-2, "Replication not active", "replication"});
    }

    if (logger_) {
        logger_->log(integrated::db_log_level::info, "Stopping replication...");
    }

    active_.store(false);

    // Wait for worker threads to finish
    if (cdc_thread_.joinable()) {
        cdc_thread_.join();
    }
    if (replication_thread_.joinable()) {
        replication_thread_.join();
    }

    // Stop CDC strategy
    if (cdc_strategy_) {
        cdc_strategy_->stop();
        cdc_strategy_.reset();
    }

    // Clean up clients
    if (source_client_) {
        source_client_->shutdown();
        source_client_.reset();
    }
    if (target_client_) {
        target_client_->shutdown();
        target_client_.reset();
    }

    // Log final statistics
    if (logger_) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        logger_->log(integrated::db_log_level::info,
            "Replication stopped. Events replicated: " +
            std::to_string(stats_.events_replicated) +
            ", Failed: " + std::to_string(stats_.events_failed) +
            ", Conflicts: " + std::to_string(stats_.conflicts_resolved));
    }

    // Shutdown observability adapters
    if (monitor_) {
        monitor_->shutdown();
        monitor_.reset();
    }
    if (logger_) {
        logger_->shutdown();
        logger_.reset();
    }

    return result<void>::ok();
}

std::chrono::milliseconds replication_manager::get_replication_lag() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.current_lag;
}

replication_stats replication_manager::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void replication_manager::set_conflict_resolution(conflict_strategy strategy) {
    config_.conflict_resolution = strategy;
}

result<void> replication_manager::trigger_replication() {
    if (!active_.load()) {
        return result<void>(error_info{-2, "Replication not active", "replication"});
    }

    if (config_.mode != sync_mode::MANUAL) {
        return result<void>(error_info{-3, "Trigger only works in MANUAL mode", "replication"});
    }

    // Force immediate processing of pending events
    paused_.store(false);

    return result<void>::ok();
}

result<void> replication_manager::pause() {
    if (!active_.load()) {
        return result<void>(error_info{-2, "Replication not active", "replication"});
    }

    paused_.store(true);

    return result<void>::ok();
}

result<void> replication_manager::resume() {
    if (!active_.load()) {
        return result<void>(error_info{-2, "Replication not active", "replication"});
    }

    paused_.store(false);

    return result<void>::ok();
}

bool replication_manager::is_healthy() const {
    if (!active_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.current_lag < HEALTH_LAG_THRESHOLD;
}

size_t replication_manager::get_pending_event_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return event_queue_.size();
}

result<void> replication_manager::configure_observability(
    const replication_observability_config& config
) {
    observability_config_ = config;

    // Shutdown existing adapters if any
    if (logger_) {
        logger_->shutdown();
        logger_.reset();
    }
    if (monitor_) {
        monitor_->shutdown();
        monitor_.reset();
    }

    // Initialize logger adapter if enabled
    if (config.enable_logging) {
        logger_ = std::make_unique<integrated::adapters::logger_adapter>(
            config.logger_config,
            integrated::adapters::logger_backend_type::auto_select
        );
        auto init_result = logger_->initialize();
        if (!init_result.is_ok()) {
            return result<void>(error_info{
                -20,
                "Failed to initialize replication logger: " + init_result.get_error().message,
                "replication"
            });
        }
        logger_->log(integrated::db_log_level::info, "Replication logger initialized");
    }

    // Initialize monitoring adapter if enabled
    if (config.enable_monitoring) {
        monitor_ = std::make_unique<integrated::adapters::monitoring_adapter>(
            config.monitoring_config,
            integrated::adapters::monitoring_backend_type::auto_select
        );
        auto init_result = monitor_->initialize();
        if (!init_result.is_ok()) {
            return result<void>(error_info{
                -21,
                "Failed to initialize replication monitoring: " + init_result.get_error().message,
                "replication"
            });
        }
        if (logger_) {
            logger_->log(integrated::db_log_level::info, "Replication monitoring initialized");
        }
    }

    return result<void>::ok();
}

result<void> replication_manager::initialize_source() {
    // Create CDC strategy based on source configuration
    // Detect database type from connection string
    cdc_strategy_ = cdc::cdc_factory::create_from_connection_string(
        source_config_.connection_string
    );

    if (!cdc_strategy_) {
        return result<void>(error_info{
            -10,
            "Unsupported database type for CDC: " + source_config_.connection_string,
            "replication"
        });
    }

    // Build list of tables to track
    std::vector<std::string> tracked_tables;
    for (const auto& mapping : config_.tables) {
        tracked_tables.push_back(mapping.source_table);
    }

    // Initialize CDC
    cdc::cdc_config cdc_config;
    cdc_config.connection_string = source_config_.connection_string;
    cdc_config.tracked_tables = tracked_tables;
    cdc_config.capture_old_values = true;
    cdc_config.max_batch_size = config_.batch_size;

    auto init_result = cdc_strategy_->initialize(cdc_config);
    if (init_result.is_err()) {
        return init_result;
    }

    // Start CDC capture
    return cdc_strategy_->start();
}

result<void> replication_manager::initialize_target() {
    // Detect database type from target connection string
    auto db_type = cdc::cdc_factory::detect_database_type(
        target_config_.connection_string
    );

    // Map CDC database_type to backend registry name
    std::string backend_name;
    switch (db_type) {
        case cdc::database_type::SQLITE:
            backend_name = "sqlite";
            break;
        case cdc::database_type::POSTGRESQL:
            backend_name = "postgresql";
            break;
        case cdc::database_type::MYSQL:
            backend_name = "mysql";
            break;
        case cdc::database_type::MONGODB:
            backend_name = "mongodb";
            break;
        default:
            return result<void>(error_info{
                -11,
                "Unsupported target database type",
                "replication"
            });
    }

    // Check if backend is available
    if (!core::backend_registry::instance().has_backend(backend_name)) {
        return result<void>(error_info{
            -12,
            "Target backend not available: " + backend_name +
                ". Ensure the backend is compiled and registered.",
            "replication"
        });
    }

    // Create target backend instance
    target_client_ = core::backend_registry::instance().create(backend_name);
    if (!target_client_) {
        return result<void>(error_info{
            -13,
            "Failed to create target backend: " + backend_name,
            "replication"
        });
    }

    // Build connection configuration
    core::connection_config conn_config;

    // Try parsing as key=value format first
    conn_config = core::connection_config::from_string(target_config_.connection_string);

    // If parsing didn't set host, use node_config fields directly
    if (conn_config.host.empty() && !target_config_.host.empty()) {
        conn_config.host = target_config_.host;
        conn_config.port = target_config_.port;
        conn_config.database = target_config_.database;
        conn_config.username = target_config_.username;
        conn_config.password = target_config_.password;
    }

    // For SQLite, store the connection string (file path) in database field
    if (db_type == cdc::database_type::SQLITE && conn_config.database.empty()) {
        // Extract file path from connection string
        std::string path = target_config_.connection_string;
        if (path.find("sqlite://") == 0) {
            path = path.substr(9);  // Remove "sqlite://" prefix
        } else if (path.find("sqlite:") == 0) {
            path = path.substr(7);  // Remove "sqlite:" prefix
        }
        conn_config.database = path;
    }

    // Initialize target client connection
    auto init_result = target_client_->initialize(conn_config);
    if (init_result.is_err()) {
        target_client_.reset();
        return result<void>(error_info{
            -14,
            "Failed to initialize target database connection: " +
                init_result.error().message,
            "replication"
        });
    }

    return result<void>::ok();
}

void replication_manager::cdc_worker() {
    while (active_.load()) {
        // Skip if paused (but keep thread alive)
        if (paused_.load()) {
            std::this_thread::sleep_for(WORKER_SLEEP_INTERVAL);
            continue;
        }

        // In MANUAL mode, wait for trigger
        if (config_.mode == sync_mode::MANUAL) {
            std::this_thread::sleep_for(WORKER_SLEEP_INTERVAL);
            continue;
        }

        // In BATCH mode, check interval
        if (config_.mode == sync_mode::BATCH) {
            static auto last_batch = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_batch < config_.batch_interval) {
                std::this_thread::sleep_for(WORKER_SLEEP_INTERVAL);
                continue;
            }
            last_batch = now;
        }

        // Capture change events from source
        auto event_opt = capture_change_event();
        if (event_opt.has_value()) {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            event_queue_.push_back(event_opt.value());

            // Limit queue size to batch_size
            if (event_queue_.size() > config_.batch_size * 2) {
                // Remove oldest events if queue grows too large
                event_queue_.erase(
                    event_queue_.begin(),
                    event_queue_.begin() + static_cast<long>(config_.batch_size)
                );
            }
        }

        std::this_thread::sleep_for(CDC_POLL_INTERVAL);
    }
}

void replication_manager::replication_worker() {
    while (active_.load()) {
        // Skip if paused
        if (paused_.load()) {
            std::this_thread::sleep_for(WORKER_SLEEP_INTERVAL);
            continue;
        }

        // Get events to process
        std::vector<replication_event> events_to_process;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (event_queue_.empty()) {
                // No events to process
            } else {
                // Process up to batch_size events
                size_t count = std::min(event_queue_.size(), config_.batch_size);
                events_to_process.assign(
                    event_queue_.begin(),
                    event_queue_.begin() + static_cast<long>(count)
                );
                event_queue_.erase(
                    event_queue_.begin(),
                    event_queue_.begin() + static_cast<long>(count)
                );
            }
        }

        // Apply each event
        for (const auto& event : events_to_process) {
            auto apply_start = std::chrono::steady_clock::now();
            auto apply_result = apply_change_event(event);
            auto apply_end = std::chrono::steady_clock::now();

            auto end_time = std::chrono::system_clock::now();
            auto lag = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - event.timestamp
            );

            // Record metrics
            if (monitor_) {
                auto apply_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    apply_end - apply_start
                );
                monitor_->record_query_execution(apply_duration, apply_result.is_ok());
                monitor_->record_metric("replication_lag_ms",
                    static_cast<double>(lag.count()));
            }

            // Log event processing result
            if (!apply_result.is_ok() && logger_) {
                logger_->log_error("apply_change_event",
                    apply_result.error().message,
                    std::to_string(apply_result.error().code));
            }

            update_stats(apply_result.is_ok(), lag);
        }

        if (events_to_process.empty()) {
            std::this_thread::sleep_for(WORKER_SLEEP_INTERVAL);
        }
    }
}

std::optional<replication_event> replication_manager::capture_change_event() {
    // Use CDC strategy to capture changes
    if (!cdc_strategy_ || !cdc_strategy_->is_active()) {
        return std::nullopt;
    }

    return cdc_strategy_->capture_next_event();
}

result<void> replication_manager::apply_change_event(const replication_event& event) {
    if (!target_client_ || !target_client_->is_initialized()) {
        return result<void>(error_info{-4, "Target not initialized", "replication"});
    }

    // Build and execute query based on event type using safe_query_builder
    // to prevent SQL injection attacks
    std::string query;

    try {
        switch (event.type) {
            case replication_event::event_type::INSERT: {
                // Build safe INSERT query with escaped values
                query = safe_query_builder::build_insert(
                    event.table_name,
                    event.new_values
                );

                auto insert_result = target_client_->insert_query(query);
                if (insert_result.is_err()) {
                    return result<void>(insert_result.error());
                }
                break;
            }

            case replication_event::event_type::UPDATE: {
                // Build safe UPDATE query with escaped values
                query = safe_query_builder::build_update(
                    event.table_name,
                    event.new_values,
                    event.old_values
                );

                auto update_result = target_client_->update_query(query);
                if (update_result.is_err()) {
                    return result<void>(update_result.error());
                }
                break;
            }

            case replication_event::event_type::DELETE: {
                // Build safe DELETE query with escaped values
                query = safe_query_builder::build_delete(
                    event.table_name,
                    event.old_values
                );

                auto delete_result = target_client_->delete_query(query);
                if (delete_result.is_err()) {
                    return result<void>(delete_result.error());
                }
                break;
            }
        }
    } catch (const std::invalid_argument& e) {
        return result<void>(error_info{-5, e.what(), "replication"});
    }

    return result<void>::ok();
}

replication_event replication_manager::resolve_conflict(const replication_event& event) {
    // Apply conflict resolution strategy
    switch (config_.conflict_resolution) {
        case conflict_strategy::LAST_WRITE_WINS:
            // Use the incoming event (latest write)
            return event;

        case conflict_strategy::FIRST_WRITE_WINS:
            // Would need to check target for existing value
            // For simplicity, return the event (real impl would query target)
            return event;

        case conflict_strategy::MANUAL:
            // Return event but mark for review
            return event;

        case conflict_strategy::CUSTOM:
            // Would invoke custom callback
            return event;

        default:
            return event;
    }
}

void replication_manager::update_stats(bool success, std::chrono::milliseconds lag) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    if (success) {
        stats_.events_replicated++;
    } else {
        stats_.events_failed++;
    }

    stats_.current_lag = lag;
    stats_.last_event_time = std::chrono::steady_clock::now();

    // Update average lag (exponential moving average)
    if (stats_.avg_lag.count() == 0) {
        stats_.avg_lag = lag;
    } else {
        stats_.avg_lag = std::chrono::milliseconds(
            (stats_.avg_lag.count() * 9 + lag.count()) / 10
        );
    }

    // Update max lag
    if (lag > stats_.max_lag) {
        stats_.max_lag = lag;
    }
}

} // namespace database::replication
