/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025
All rights reserved.
*****************************************************************************/

#include "mongodb_cdc_strategy.h"

#ifdef USE_MONGODB
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#endif

#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <regex>

namespace database::replication::cdc {

namespace {

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
mongodb_cdc_strategy::mongodb_cdc_strategy() {
#ifdef USE_MONGODB
    // Initialize MongoDB C driver if not already done
    static bool mongoc_initialized = false;
    if (!mongoc_initialized) {
        mongoc_init();
        mongoc_initialized = true;
    }
#endif
}

// Destructor
mongodb_cdc_strategy::~mongodb_cdc_strategy() {
    if (active_.load()) {
        stop();
    }
#ifdef USE_MONGODB
    if (client_) {
        mongoc_client_destroy(static_cast<mongoc_client_t*>(client_));
        client_ = nullptr;
    }
#endif
}

// Move constructor
mongodb_cdc_strategy::mongodb_cdc_strategy(mongodb_cdc_strategy&& other) noexcept
    : config_(std::move(other.config_)),
      uri_(std::move(other.uri_)),
      database_name_(std::move(other.database_name_)),
      resume_token_(std::move(other.resume_token_)),
      active_(other.active_.load()),
      initialized_(other.initialized_.load()),
      stop_requested_(other.stop_requested_.load()),
      tracked_collections_(std::move(other.tracked_collections_)),
      client_(other.client_) {
    other.client_ = nullptr;
    other.active_.store(false);
    other.initialized_.store(false);
}

// Move assignment
mongodb_cdc_strategy& mongodb_cdc_strategy::operator=(mongodb_cdc_strategy&& other) noexcept {
    if (this != &other) {
#ifdef USE_MONGODB
        if (client_) {
            mongoc_client_destroy(static_cast<mongoc_client_t*>(client_));
        }
#endif
        config_ = std::move(other.config_);
        uri_ = std::move(other.uri_);
        database_name_ = std::move(other.database_name_);
        resume_token_ = std::move(other.resume_token_);
        active_.store(other.active_.load());
        initialized_.store(other.initialized_.load());
        stop_requested_.store(other.stop_requested_.load());
        tracked_collections_ = std::move(other.tracked_collections_);
        client_ = other.client_;

        other.client_ = nullptr;
        other.active_.store(false);
        other.initialized_.store(false);
    }
    return *this;
}

void mongodb_cdc_strategy::parse_connection_string() {
    uri_ = config_.connection_string;

    // Extract database name from URI
    // Format: mongodb://[user:pass@]host[:port]/database[?options]
    std::regex uri_regex(R"(mongodb(?:\+srv)?://[^/]+/([^?]+))");
    std::smatch matches;

    if (std::regex_search(uri_, matches, uri_regex) && matches.size() > 1) {
        database_name_ = matches[1].str();
    }

    if (database_name_.empty()) {
        database_name_ = "test";  // Default database
    }
}

result<void> mongodb_cdc_strategy::initialize(const cdc_config& config) {
#ifndef USE_MONGODB
    (void)config;
    return result<void>(error_info{-1, "MongoDB support not compiled", "mongodb_cdc"});
#else
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_.load()) {
        return result<void>(error_info{-1, "CDC already initialized", "mongodb_cdc"});
    }

    config_ = config;
    parse_connection_string();

    // Create MongoDB client
    bson_error_t error;
    mongoc_uri_t* uri = mongoc_uri_new_with_error(uri_.c_str(), &error);
    if (!uri) {
        return result<void>(error_info{-2, std::string("Invalid URI: ") + error.message, "mongodb_cdc"});
    }

    client_ = mongoc_client_new_from_uri(uri);
    mongoc_uri_destroy(uri);

    if (!client_) {
        return result<void>(error_info{-3, "Failed to create MongoDB client", "mongodb_cdc"});
    }

    // Set application name
    mongoc_client_set_appname(static_cast<mongoc_client_t*>(client_), "cdc_strategy");

    // Verify connection by pinging the server
    bson_t ping_cmd = BSON_INITIALIZER;
    BSON_APPEND_INT32(&ping_cmd, "ping", 1);

    bson_t reply;
    bool success = mongoc_client_command_simple(
        static_cast<mongoc_client_t*>(client_),
        "admin",
        &ping_cmd,
        nullptr,
        &reply,
        &error
    );

    bson_destroy(&ping_cmd);
    bson_destroy(&reply);

    if (!success) {
        mongoc_client_destroy(static_cast<mongoc_client_t*>(client_));
        client_ = nullptr;
        return result<void>(error_info{-4, std::string("Cannot connect to MongoDB: ") + error.message, "mongodb_cdc"});
    }

    // Store tracked collections
    for (const auto& collection : config.tracked_tables) {
        tracked_collections_.insert(collection);
    }

    initialized_.store(true);
    return result<void>::ok();
#endif
}

result<void> mongodb_cdc_strategy::start() {
#ifndef USE_MONGODB
    return result<void>(error_info{-1, "MongoDB support not compiled", "mongodb_cdc"});
#else
    if (!initialized_.load()) {
        return result<void>(error_info{-5, "CDC not initialized", "mongodb_cdc"});
    }

    if (active_.load()) {
        return result<void>(error_info{-6, "CDC already active", "mongodb_cdc"});
    }

    stop_requested_.store(false);
    active_.store(true);

    // Start change stream worker thread
    change_stream_thread_ = std::thread(&mongodb_cdc_strategy::change_stream_worker, this);

    return result<void>::ok();
#endif
}

result<void> mongodb_cdc_strategy::stop() {
#ifndef USE_MONGODB
    return result<void>(error_info{-1, "MongoDB support not compiled", "mongodb_cdc"});
#else
    if (!active_.load()) {
        return result<void>(error_info{-7, "CDC not active", "mongodb_cdc"});
    }

    stop_requested_.store(true);

    if (change_stream_thread_.joinable()) {
        change_stream_thread_.join();
    }

    active_.store(false);
    return result<void>::ok();
#endif
}

std::optional<replication_event> mongodb_cdc_strategy::capture_next_event() {
    auto events = capture_events(1);
    if (events.empty()) {
        return std::nullopt;
    }
    return events[0];
}

std::vector<replication_event> mongodb_cdc_strategy::capture_events(size_t max_count) {
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

result<void> mongodb_cdc_strategy::acknowledge_event(const replication_event& /*event*/) {
    // Resume token is automatically tracked during change stream iteration
    return result<void>::ok();
}

std::string mongodb_cdc_strategy::get_current_position() const {
    return resume_token_;
}

result<void> mongodb_cdc_strategy::set_position(const std::string& position) {
    resume_token_ = position;
    return result<void>::ok();
}

bool mongodb_cdc_strategy::is_active() const {
    return active_.load();
}

database_type mongodb_cdc_strategy::get_database_type() const {
    return database_type::MONGODB;
}

result<void> mongodb_cdc_strategy::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    tracked_collections_.clear();
    resume_token_.clear();
    initialized_.store(false);
    active_.store(false);

    return result<void>::ok();
}

size_t mongodb_cdc_strategy::get_pending_count() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return event_queue_.size();
}

void mongodb_cdc_strategy::change_stream_worker() {
#ifdef USE_MONGODB
    auto* mongo_client = static_cast<mongoc_client_t*>(client_);
    if (!mongo_client) {
        active_.store(false);
        return;
    }

    // Build pipeline for filtering collections if specified
    bson_t pipeline = BSON_INITIALIZER;
    bson_t pipeline_array;
    BSON_APPEND_ARRAY_BEGIN(&pipeline, "pipeline", &pipeline_array);

    if (!tracked_collections_.empty()) {
        // Add $match stage to filter by collection
        bson_t match_stage;
        BSON_APPEND_DOCUMENT_BEGIN(&pipeline_array, "0", &match_stage);

        bson_t match_doc;
        BSON_APPEND_DOCUMENT_BEGIN(&match_stage, "$match", &match_doc);

        bson_t ns_coll;
        BSON_APPEND_DOCUMENT_BEGIN(&match_doc, "ns.coll", &ns_coll);

        bson_t in_array;
        BSON_APPEND_ARRAY_BEGIN(&ns_coll, "$in", &in_array);

        int idx = 0;
        for (const auto& coll : tracked_collections_) {
            char key[16];
            snprintf(key, sizeof(key), "%d", idx++);
            BSON_APPEND_UTF8(&in_array, key, coll.c_str());
        }

        bson_append_array_end(&ns_coll, &in_array);
        bson_append_document_end(&match_doc, &ns_coll);
        bson_append_document_end(&match_stage, &match_doc);
        bson_append_document_end(&pipeline_array, &match_stage);
    }

    bson_append_array_end(&pipeline, &pipeline_array);

    // Build options
    bson_t opts = BSON_INITIALIZER;
    BSON_APPEND_UTF8(&opts, "fullDocument", "updateLookup");

    // Resume from token if available
    if (!resume_token_.empty()) {
        bson_t resume_after;
        bson_error_t error;
        if (bson_init_from_json(&resume_after, resume_token_.c_str(),
                                static_cast<ssize_t>(resume_token_.size()), &error)) {
            BSON_APPEND_DOCUMENT(&opts, "resumeAfter", &resume_after);
            bson_destroy(&resume_after);
        }
    }

    // Get database and open change stream
    mongoc_database_t* db = mongoc_client_get_database(mongo_client, database_name_.c_str());
    mongoc_change_stream_t* stream = mongoc_database_watch(db, &pipeline, &opts);

    bson_destroy(&pipeline);
    bson_destroy(&opts);

    if (!stream) {
        mongoc_database_destroy(db);
        active_.store(false);
        return;
    }

    // Main change stream loop
    const bson_t* doc;
    bson_error_t error;

    while (!stop_requested_.load()) {
        // Try to get next change with timeout
        if (mongoc_change_stream_next(stream, &doc)) {
            // Parse the change document
            char* json = bson_as_json(doc, nullptr);
            if (json) {
                // Extract operation type, collection, document info
                bson_iter_t iter;
                std::string operation_type;
                std::string collection;
                std::string document_key;
                std::string full_document;
                std::string update_description;

                if (bson_iter_init(&iter, doc)) {
                    while (bson_iter_next(&iter)) {
                        const char* key = bson_iter_key(&iter);

                        if (strcmp(key, "operationType") == 0 && BSON_ITER_HOLDS_UTF8(&iter)) {
                            operation_type = bson_iter_utf8(&iter, nullptr);
                        } else if (strcmp(key, "ns") == 0 && BSON_ITER_HOLDS_DOCUMENT(&iter)) {
                            bson_iter_t ns_iter;
                            if (bson_iter_recurse(&iter, &ns_iter)) {
                                while (bson_iter_next(&ns_iter)) {
                                    if (strcmp(bson_iter_key(&ns_iter), "coll") == 0 &&
                                        BSON_ITER_HOLDS_UTF8(&ns_iter)) {
                                        collection = bson_iter_utf8(&ns_iter, nullptr);
                                    }
                                }
                            }
                        } else if (strcmp(key, "documentKey") == 0 && BSON_ITER_HOLDS_DOCUMENT(&iter)) {
                            uint32_t len;
                            const uint8_t* data;
                            bson_iter_document(&iter, &len, &data);
                            bson_t* subdoc = bson_new_from_data(data, len);
                            if (subdoc) {
                                char* subdoc_json = bson_as_json(subdoc, nullptr);
                                if (subdoc_json) {
                                    document_key = subdoc_json;
                                    bson_free(subdoc_json);
                                }
                                bson_destroy(subdoc);
                            }
                        } else if (strcmp(key, "fullDocument") == 0 && BSON_ITER_HOLDS_DOCUMENT(&iter)) {
                            uint32_t len;
                            const uint8_t* data;
                            bson_iter_document(&iter, &len, &data);
                            bson_t* subdoc = bson_new_from_data(data, len);
                            if (subdoc) {
                                char* subdoc_json = bson_as_json(subdoc, nullptr);
                                if (subdoc_json) {
                                    full_document = subdoc_json;
                                    bson_free(subdoc_json);
                                }
                                bson_destroy(subdoc);
                            }
                        } else if (strcmp(key, "updateDescription") == 0 && BSON_ITER_HOLDS_DOCUMENT(&iter)) {
                            uint32_t len;
                            const uint8_t* data;
                            bson_iter_document(&iter, &len, &data);
                            bson_t* subdoc = bson_new_from_data(data, len);
                            if (subdoc) {
                                char* subdoc_json = bson_as_json(subdoc, nullptr);
                                if (subdoc_json) {
                                    update_description = subdoc_json;
                                    bson_free(subdoc_json);
                                }
                                bson_destroy(subdoc);
                            }
                        } else if (strcmp(key, "_id") == 0 && BSON_ITER_HOLDS_DOCUMENT(&iter)) {
                            // This is the resume token
                            uint32_t len;
                            const uint8_t* data;
                            bson_iter_document(&iter, &len, &data);
                            bson_t* subdoc = bson_new_from_data(data, len);
                            if (subdoc) {
                                char* subdoc_json = bson_as_json(subdoc, nullptr);
                                if (subdoc_json) {
                                    resume_token_ = subdoc_json;
                                    bson_free(subdoc_json);
                                }
                                bson_destroy(subdoc);
                            }
                        }
                    }
                }

                // Create replication event
                if (!operation_type.empty()) {
                    auto event = parse_change_event(
                        operation_type,
                        collection,
                        document_key,
                        full_document,
                        update_description
                    );

                    // Add to queue
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    event_queue_.push(std::move(event));
                }

                bson_free(json);
            }
        } else {
            // Check for error
            if (mongoc_change_stream_error_document(stream, &error, nullptr)) {
                // Log error and continue
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } else {
                // No data available, wait a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    mongoc_change_stream_destroy(stream);
    mongoc_database_destroy(db);
#endif
}

replication_event mongodb_cdc_strategy::parse_change_event(
    const std::string& operation_type,
    const std::string& collection,
    const std::string& document_key,
    const std::string& full_document,
    const std::string& update_description) {

    replication_event event;
    event.table_name = collection;
    event.timestamp = std::chrono::system_clock::now();

    if (operation_type == "insert") {
        event.type = replication_event::event_type::INSERT;
        event.new_values["_document"] = full_document;
        event.new_values["_key"] = document_key;
    } else if (operation_type == "update" || operation_type == "replace") {
        event.type = replication_event::event_type::UPDATE;
        event.new_values["_document"] = full_document;
        event.new_values["_key"] = document_key;
        event.new_values["_update"] = update_description;
    } else if (operation_type == "delete") {
        event.type = replication_event::event_type::DELETE;
        event.old_values["_key"] = document_key;
    } else {
        // Unknown operation type, treat as insert
        event.type = replication_event::event_type::INSERT;
        event.new_values["_operation"] = operation_type;
    }

    return event;
}

} // namespace database::replication::cdc
