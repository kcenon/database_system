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

// common_system integration (required)
#include <kcenon/common/patterns/result.h>
#include <kcenon/common/interfaces/database_interface.h>
#include <kcenon/common/adapters/typed_adapter.h>

#include "../database_base.h"
#include "../database_manager.h"

namespace database {
namespace adapters {

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
 *
 * Now inherits from typed_adapter for:
 * - Type safety and wrapper depth tracking
 * - Automatic prevention of infinite adapter chains (max depth: 2)
 * - Unwrap support to access underlying database_base
 */
class common_system_database_adapter
    : public ::common::adapters::typed_adapter<::common::interfaces::IDatabase, database_base> {
    using base_type = ::common::adapters::typed_adapter<::common::interfaces::IDatabase, database_base>;

public:
    /**
     * @brief Construct adapter with database_base instance
     */
    explicit common_system_database_adapter(
        std::shared_ptr<database_base> db)
        : base_type(db) {}

    ~common_system_database_adapter() override = default;

    /**
     * @brief Connect to database
     */
    ::common::VoidResult connect(const std::string& connection_string) override {
        if (!this->impl_) {
            return to_common_error(1, "Database not initialized");
        }

        if (this->impl_->connect(connection_string)) {
            return to_common_result_void();
        } else {
            return to_common_error(2, "Failed to connect to database");
        }
    }

    /**
     * @brief Disconnect from database
     */
    ::common::VoidResult disconnect() override {
        if (!this->impl_) {
            return to_common_error(1, "Database not initialized");
        }

        this->impl_->disconnect();
        return to_common_result_void();
    }

    /**
     * @brief Execute a query and return results
     */
    ::common::Result<::common::database_result> execute_query(
        const std::string& query) override {
        if (!this->impl_) {
            return ::common::error_info(1, "Database not initialized", "database_system");
        }

        auto result = this->impl_->execute_query(query);

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
        if (!this->impl_) {
            return to_common_error(1, "Database not initialized");
        }

        if (this->impl_->execute(command)) {
            return to_common_result_void();
        } else {
            return to_common_error(3, "Failed to execute command");
        }
    }

    /**
     * @brief Begin a transaction
     */
    ::common::VoidResult begin_transaction() override {
        if (!this->impl_) {
            return to_common_error(1, "Database not initialized");
        }

        if (this->impl_->begin_transaction()) {
            return to_common_result_void();
        } else {
            return to_common_error(4, "Failed to begin transaction");
        }
    }

    /**
     * @brief Commit current transaction
     */
    ::common::VoidResult commit() override {
        if (!this->impl_) {
            return to_common_error(1, "Database not initialized");
        }

        if (this->impl_->commit()) {
            return to_common_result_void();
        } else {
            return to_common_error(5, "Failed to commit transaction");
        }
    }

    /**
     * @brief Rollback current transaction
     */
    ::common::VoidResult rollback() override {
        if (!this->impl_) {
            return to_common_error(1, "Database not initialized");
        }

        if (this->impl_->rollback()) {
            return to_common_result_void();
        } else {
            return to_common_error(6, "Failed to rollback transaction");
        }
    }

    /**
     * @brief Check if connected to database
     */
    bool is_connected() const override {
        return this->impl_ && this->impl_->is_connected();
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

} // namespace adapters
} // namespace database