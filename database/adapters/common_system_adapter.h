#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, database_system contributors
All rights reserved.
*****************************************************************************/

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <future>

// Check if common_system is available (support both flags)
#if defined(BUILD_WITH_COMMON_SYSTEM) || defined(DATABASE_USE_COMMON_SYSTEM)
#include <kcenon/common/patterns/result.h>
#include <kcenon/common/interfaces/database_interface.h>
#endif

#include "../database_base.h"
#include "../database_manager.h"
#include "../connection_pool.h"

namespace database {
namespace adapters {

#if defined(BUILD_WITH_COMMON_SYSTEM) || defined(DATABASE_USE_COMMON_SYSTEM)

/**
 * @brief Result type conversions between database and common_system
 */
template<typename T>
inline ::common::Result<T> to_common_result(const T& value) {
    return ::common::Result<T>(value);
}

inline ::common::VoidResult to_common_result_void() {
    return ::common::VoidResult(std::monostate{});
}

inline ::common::VoidResult to_common_error(int code, const std::string& msg) {
    return ::common::VoidResult(::common::error_info(code, msg, "database_system"));
}

/**
 * @brief Convert database_value to common system variant type
 */
inline ::common::database_value to_common_value(const database_value& value) {
    return std::visit([](auto&& arg) -> ::common::database_value {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
            return ::common::database_null{};
        } else {
            return arg;
        }
    }, value);
}

/**
 * @brief Convert common system database_value to database_value
 */
inline database_value from_common_value(const ::common::database_value& value) {
    return std::visit([](auto&& arg) -> database_value {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ::common::database_null>) {
            return nullptr;
        } else {
            return arg;
        }
    }, value);
}

/**
 * @brief Adapter to expose database_base as common::interfaces::IDatabase
 */
class common_system_database_adapter : public ::common::interfaces::IDatabase {
public:
    /**
     * @brief Construct adapter with database_base instance
     */
    explicit common_system_database_adapter(
        std::shared_ptr<database_base> db)
        : database_(db) {}

    ~common_system_database_adapter() override = default;

    /**
     * @brief Connect to database
     */
    ::common::VoidResult connect(const std::string& connection_string) override {
        if (!database_) {
            return to_common_error(1, "Database not initialized");
        }

        if (database_->connect(connection_string)) {
            return to_common_result_void();
        } else {
            return to_common_error(2, "Failed to connect to database");
        }
    }

    /**
     * @brief Disconnect from database
     */
    ::common::VoidResult disconnect() override {
        if (!database_) {
            return to_common_error(1, "Database not initialized");
        }

        database_->disconnect();
        return to_common_result_void();
    }

    /**
     * @brief Execute a query and return results
     */
    ::common::Result<::common::database_result> execute_query(
        const std::string& query) override {
        if (!database_) {
            return ::common::error_info(1, "Database not initialized", "database_system");
        }

        auto result = database_->execute_query(query);

        // Convert database_result to common::database_result
        ::common::database_result common_result;
        for (const auto& row : result) {
            ::common::database_row common_row;
            for (const auto& [key, value] : row) {
                common_row[key] = to_common_value(value);
            }
            common_result.push_back(common_row);
        }

        return to_common_result(common_result);
    }

    /**
     * @brief Execute a command (no results expected)
     */
    ::common::VoidResult execute_command(const std::string& command) override {
        if (!database_) {
            return to_common_error(1, "Database not initialized");
        }

        if (database_->execute(command)) {
            return to_common_result_void();
        } else {
            return to_common_error(3, "Failed to execute command");
        }
    }

    /**
     * @brief Begin a transaction
     */
    ::common::VoidResult begin_transaction() override {
        if (!database_) {
            return to_common_error(1, "Database not initialized");
        }

        if (database_->begin_transaction()) {
            return to_common_result_void();
        } else {
            return to_common_error(4, "Failed to begin transaction");
        }
    }

    /**
     * @brief Commit current transaction
     */
    ::common::VoidResult commit() override {
        if (!database_) {
            return to_common_error(1, "Database not initialized");
        }

        if (database_->commit()) {
            return to_common_result_void();
        } else {
            return to_common_error(5, "Failed to commit transaction");
        }
    }

    /**
     * @brief Rollback current transaction
     */
    ::common::VoidResult rollback() override {
        if (!database_) {
            return to_common_error(1, "Database not initialized");
        }

        if (database_->rollback()) {
            return to_common_result_void();
        } else {
            return to_common_error(6, "Failed to rollback transaction");
        }
    }

    /**
     * @brief Check if connected to database
     */
    bool is_connected() const override {
        return database_ && database_->is_connected();
    }

private:
    std::shared_ptr<database_base> database_;
};

/**
 * @brief Adapter for connection pool with common_system executor
 */
class common_connection_pool_adapter {
public:
    /**
     * @brief Create connection pool with common_system executor
     */
    static std::shared_ptr<connection_pool> create_with_common_executor(
        std::shared_ptr<::common::interfaces::IExecutor> executor,
        const connection_pool::config& cfg) {

        // Create a connection pool that can use common executor
        // for async operations
        auto pool = std::make_shared<connection_pool>(cfg);

        // Note: The actual integration would require modifying
        // connection_pool to accept an executor, but we provide
        // the interface here

        return pool;
    }
};

/**
 * @brief Factory for creating common_system compatible databases
 */
class common_database_factory {
public:
    /**
     * @brief Create a common_system IDatabase from database_base
     */
    static std::shared_ptr<::common::interfaces::IDatabase> create_common_database(
        std::shared_ptr<database_base> db) {
        return std::make_shared<common_system_database_adapter>(db);
    }

    /**
     * @brief Create common_system IDatabase from database_manager singleton
     */
    static std::shared_ptr<::common::interfaces::IDatabase> create_from_manager() {
        auto& manager = database_manager::instance();
        // Note: database_manager would need to expose its internal database
        // This is a design consideration for the actual implementation
        return nullptr; // Placeholder
    }
};

#endif // BUILD_WITH_COMMON_SYSTEM || DATABASE_USE_COMMON_SYSTEM

} // namespace adapters
} // namespace database