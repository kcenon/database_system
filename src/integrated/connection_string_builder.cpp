// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/database/integrated/connection_string_builder.h>

#include <sstream>

namespace kcenon::database::integrated {

connection_string_builder& connection_string_builder::host(std::string_view h) {
    host_ = std::string(h);
    return *this;
}

connection_string_builder& connection_string_builder::port(uint16_t p) {
    port_ = p;
    return *this;
}

connection_string_builder& connection_string_builder::database(std::string_view db) {
    database_ = std::string(db);
    return *this;
}

connection_string_builder& connection_string_builder::user(std::string_view u) {
    user_ = std::string(u);
    return *this;
}

connection_string_builder& connection_string_builder::password(std::string_view p) {
    password_ = std::string(p);
    return *this;
}

connection_string_builder& connection_string_builder::ssl_mode(enum ssl_mode mode) {
    ssl_mode_ = mode;
    return *this;
}

connection_string_builder& connection_string_builder::connect_timeout(uint32_t seconds) {
    connect_timeout_ = seconds;
    return *this;
}

connection_string_builder& connection_string_builder::application_name(std::string_view name) {
    application_name_ = std::string(name);
    return *this;
}

connection_string_builder& connection_string_builder::in_memory() {
    in_memory_ = true;
    return *this;
}

connection_string_builder& connection_string_builder::option(std::string_view key, std::string_view value) {
    custom_options_.emplace_back(std::string(key), std::string(value));
    return *this;
}

kcenon::common::Result<std::string> connection_string_builder::build(backend_type type) const {
    switch (type) {
        case backend_type::postgres:
            return build_postgres();
        case backend_type::sqlite:
            return build_sqlite();
        case backend_type::mongodb:
            return build_mongodb();
        case backend_type::redis:
            return build_redis();
        default:
            return kcenon::common::Result<std::string>::err(
                -1, "Unknown backend type", "connection_string_builder");
    }
}

connection_string_builder& connection_string_builder::reset() {
    host_.reset();
    port_.reset();
    database_.reset();
    user_.reset();
    password_.reset();
    ssl_mode_.reset();
    connect_timeout_.reset();
    application_name_.reset();
    in_memory_ = false;
    custom_options_.clear();
    return *this;
}

kcenon::common::Result<std::string> connection_string_builder::build_postgres() const {
    std::ostringstream oss;
    bool first = true;

    auto append = [&oss, &first](const std::string& key, const std::string& value) {
        if (!first) {
            oss << ' ';
        }
        oss << key << '=' << value;
        first = false;
    };

    // PostgreSQL uses space-separated key=value pairs
    if (host_.has_value()) {
        append("host", *host_);
    }

    if (port_.has_value()) {
        append("port", std::to_string(*port_));
    }

    if (database_.has_value()) {
        append("dbname", *database_);
    }

    if (user_.has_value()) {
        append("user", *user_);
    }

    if (password_.has_value()) {
        append("password", *password_);
    }

    if (ssl_mode_.has_value()) {
        append("sslmode", ssl_mode_to_postgres_string(*ssl_mode_));
    }

    if (connect_timeout_.has_value()) {
        append("connect_timeout", std::to_string(*connect_timeout_));
    }

    if (application_name_.has_value()) {
        append("application_name", *application_name_);
    }

    for (const auto& [key, value] : custom_options_) {
        append(key, value);
    }

    return kcenon::common::Result<std::string>::ok(oss.str());
}

kcenon::common::Result<std::string> connection_string_builder::build_sqlite() const {
    // SQLite uses file path or :memory: for in-memory database
    if (in_memory_) {
        return kcenon::common::Result<std::string>::ok(":memory:");
    }

    if (!database_.has_value() || database_->empty()) {
        return kcenon::common::Result<std::string>::err(
            -1, "SQLite requires a database file path or in_memory() to be set",
            "connection_string_builder");
    }

    return kcenon::common::Result<std::string>::ok(*database_);
}

kcenon::common::Result<std::string> connection_string_builder::build_mongodb() const {
    // MongoDB uses URI format: mongodb://[user:password@]host[:port]/[database]
    std::ostringstream oss;
    oss << "mongodb://";

    if (user_.has_value() && password_.has_value()) {
        oss << *user_ << ':' << *password_ << '@';
    }

    if (host_.has_value()) {
        oss << *host_;
    } else {
        oss << "localhost";
    }

    if (port_.has_value()) {
        oss << ':' << *port_;
    }

    if (database_.has_value()) {
        oss << '/' << *database_;
    }

    // Add options as query parameters
    if (!custom_options_.empty() || ssl_mode_.has_value() || connect_timeout_.has_value()) {
        oss << '?';
        bool first = true;

        auto append_param = [&oss, &first](const std::string& key, const std::string& value) {
            if (!first) {
                oss << '&';
            }
            oss << key << '=' << value;
            first = false;
        };

        if (ssl_mode_.has_value()) {
            bool use_ssl = *ssl_mode_ != ssl_mode::disable;
            append_param("ssl", use_ssl ? "true" : "false");
        }

        if (connect_timeout_.has_value()) {
            append_param("connectTimeoutMS", std::to_string(*connect_timeout_ * 1000));
        }

        for (const auto& [key, value] : custom_options_) {
            append_param(key, value);
        }
    }

    return kcenon::common::Result<std::string>::ok(oss.str());
}

kcenon::common::Result<std::string> connection_string_builder::build_redis() const {
    // Redis uses URI format: redis://[user:password@]host[:port][/database]
    std::ostringstream oss;
    oss << "redis://";

    if (user_.has_value() && password_.has_value()) {
        oss << *user_ << ':' << *password_ << '@';
    } else if (password_.has_value()) {
        // Redis often uses just password without username
        oss << ':' << *password_ << '@';
    }

    if (host_.has_value()) {
        oss << *host_;
    } else {
        oss << "localhost";
    }

    if (port_.has_value()) {
        oss << ':' << *port_;
    }

    if (database_.has_value()) {
        // Redis database is a number (0-15 typically)
        oss << '/' << *database_;
    }

    return kcenon::common::Result<std::string>::ok(oss.str());
}

std::string connection_string_builder::ssl_mode_to_postgres_string(enum ssl_mode mode) {
    switch (mode) {
        case ssl_mode::disable:
            return "disable";
        case ssl_mode::allow:
            return "allow";
        case ssl_mode::prefer:
            return "prefer";
        case ssl_mode::require:
            return "require";
        case ssl_mode::verify_ca:
            return "verify-ca";
        case ssl_mode::verify_full:
            return "verify-full";
        default:
            return "prefer";
    }
}

} // namespace kcenon::database::integrated
