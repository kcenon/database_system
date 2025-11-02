/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_simple_v2.cpp
 * @brief Simple test for connection_pool_v2
 */

#include "database/pooling/connection_pool_v2.h"
#include "database/database_types.h"
#include <iostream>

using namespace database;
using namespace database::pooling;

// Mock database
class mock_database : public database_base {
public:
    database_types database_type() override { return database_types::postgres; }
    bool connect(const std::string&) override { return true; }
    bool create_query(const std::string&) override { return true; }
    unsigned int insert_query(const std::string&) override { return 1; }
    unsigned int update_query(const std::string&) override { return 1; }
    unsigned int delete_query(const std::string&) override { return 1; }
    database_result select_query(const std::string&) override { return database_result{}; }
    bool execute_query(const std::string&) override { return true; }
    bool disconnect() override { return true; }
};

int main() {
    std::cout << "Starting simple test...\n";

    try {
        // Configure pool
        connection_pool_config config;
        config.min_connections = 1;
        config.max_connections = 2;
        config.connection_string = "mock://localhost";

        std::cout << "Creating factory...\n";
        auto factory = []() -> std::unique_ptr<database_base> {
            return std::make_unique<mock_database>();
        };

        std::cout << "Creating connection_pool_v2...\n";
        connection_pool_v2 pool(database_types::postgres, config, factory, 2);

        std::cout << "Initializing pool...\n";
        if (!pool.initialize()) {
            std::cerr << "Failed to initialize pool\n";
            return 1;
        }

        std::cout << "Pool initialized successfully!\n";
        std::cout << "Using thread_system: " << (pool.is_using_thread_system() ? "YES" : "NO") << "\n";

        std::cout << "Shutting down...\n";
        pool.shutdown();

        std::cout << "Test completed successfully!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
