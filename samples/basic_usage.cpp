/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Database System Project
 */

#include <iostream>
#include <string>
#include <memory>
#include "../database_manager.h"
#include "../postgres_manager.h"

using namespace database_module;

int main() {
    std::cout << "=== Database System - Basic Usage Example ===" << std::endl;
    
    // 1. Database manager creation and configuration
    std::cout << "\n1. Database Manager Setup:" << std::endl;
    
    auto db_manager = std::make_shared<database_manager>();
    
    // Set database type
    db_manager->set_database_type(database_types::postgres);
    std::cout << "Database type set to: PostgreSQL" << std::endl;
    
    // Connection string (modify these values for your database)
    std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
    std::cout << "Connection string configured" << std::endl;
    
    // Note: This example shows the API usage, but requires an actual PostgreSQL server
    std::cout << "Note: This example demonstrates API usage. Actual database connection requires PostgreSQL server." << std::endl;
    
    // 2. Connection management
    std::cout << "\n2. Connection Management:" << std::endl;
    
    std::cout << "Attempting to connect to database..." << std::endl;
    bool connected = db_manager->connect(connection_string);
    
    if (connected) {
        std::cout << "✓ Successfully connected to database" << std::endl;
        std::cout << "Connection status: " << (db_manager->is_connected() ? "Connected" : "Disconnected") << std::endl;
        std::cout << "Database type: " << static_cast<int>(db_manager->get_database_type()) << std::endl;
        
        // 3. Table operations
        std::cout << "\n3. Table Operations:" << std::endl;
        
        // Create table
        std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                username VARCHAR(50) UNIQUE NOT NULL,
                email VARCHAR(100) UNIQUE NOT NULL,
                age INTEGER CHECK (age >= 0),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                is_active BOOLEAN DEFAULT TRUE
            )
        )";
        
        std::cout << "Creating users table..." << std::endl;
        bool table_created = db_manager->create_query(create_table_sql);
        if (table_created) {
            std::cout << "✓ Users table created successfully" << std::endl;
        } else {
            std::cout << "✗ Failed to create users table" << std::endl;
        }
        
        // 4. Data insertion
        std::cout << "\n4. Data Insertion:" << std::endl;
        
        std::vector<std::string> insert_queries = {
            "INSERT INTO users (username, email, age) VALUES ('john_doe', 'john@example.com', 30)",
            "INSERT INTO users (username, email, age) VALUES ('jane_smith', 'jane@example.com', 25)",
            "INSERT INTO users (username, email, age) VALUES ('bob_wilson', 'bob@example.com', 35)",
            "INSERT INTO users (username, email, age, is_active) VALUES ('alice_brown', 'alice@example.com', 28, FALSE)"
        };
        
        for (const auto& query : insert_queries) {
            bool inserted = db_manager->insert_query(query);
            if (inserted) {
                std::cout << "✓ User inserted successfully" << std::endl;
            } else {
                std::cout << "✗ Failed to insert user (may already exist)" << std::endl;
            }
        }
        
        // 5. Data selection
        std::cout << "\n5. Data Selection:" << std::endl;
        
        std::string select_all = "SELECT id, username, email, age, is_active FROM users ORDER BY id";
        auto all_users = db_manager->select_query(select_all);
        
        if (all_users) {
            std::cout << "✓ All users retrieved:" << std::endl;
            std::cout << *all_users << std::endl;
        } else {
            std::cout << "✗ Failed to retrieve users" << std::endl;
        }
        
        // Select specific user
        std::string select_user = "SELECT username, email, age FROM users WHERE username = 'john_doe'";
        auto john_data = db_manager->select_query(select_user);
        
        if (john_data) {
            std::cout << "✓ John's data retrieved:" << std::endl;
            std::cout << *john_data << std::endl;
        } else {
            std::cout << "✗ John's data not found" << std::endl;
        }
        
        // 6. Data updates
        std::cout << "\n6. Data Updates:" << std::endl;
        
        std::string update_query = "UPDATE users SET age = 31 WHERE username = 'john_doe'";
        bool updated = db_manager->update_query(update_query);
        
        if (updated) {
            std::cout << "✓ John's age updated successfully" << std::endl;
            
            // Verify update
            auto updated_data = db_manager->select_query("SELECT username, age FROM users WHERE username = 'john_doe'");
            if (updated_data) {
                std::cout << "Updated data: " << *updated_data << std::endl;
            }
        } else {
            std::cout << "✗ Failed to update John's age" << std::endl;
        }
        
        // 7. Transaction management
        std::cout << "\n7. Transaction Management:" << std::endl;
        
        std::cout << "Starting transaction..." << std::endl;
        bool transaction_started = db_manager->begin_transaction();
        
        if (transaction_started) {
            std::cout << "✓ Transaction started" << std::endl;
            std::cout << "In transaction: " << (db_manager->is_in_transaction() ? "Yes" : "No") << std::endl;
            
            // Perform operations within transaction
            bool op1 = db_manager->insert_query("INSERT INTO users (username, email, age) VALUES ('temp_user1', 'temp1@example.com', 40)");
            bool op2 = db_manager->insert_query("INSERT INTO users (username, email, age) VALUES ('temp_user2', 'temp2@example.com', 45)");
            
            if (op1 && op2) {
                std::cout << "✓ Transaction operations successful, committing..." << std::endl;
                bool committed = db_manager->commit_transaction();
                if (committed) {
                    std::cout << "✓ Transaction committed successfully" << std::endl;
                } else {
                    std::cout << "✗ Failed to commit transaction" << std::endl;
                }
            } else {
                std::cout << "✗ Transaction operations failed, rolling back..." << std::endl;
                bool rolled_back = db_manager->rollback_transaction();
                if (rolled_back) {
                    std::cout << "✓ Transaction rolled back successfully" << std::endl;
                } else {
                    std::cout << "✗ Failed to rollback transaction" << std::endl;
                }
            }
        } else {
            std::cout << "✗ Failed to start transaction" << std::endl;
        }
        
        // 8. Data deletion
        std::cout << "\n8. Data Deletion:" << std::endl;
        
        std::string delete_query = "DELETE FROM users WHERE username LIKE 'temp_user%'";
        bool deleted = db_manager->delete_query(delete_query);
        
        if (deleted) {
            std::cout << "✓ Temporary users deleted successfully" << std::endl;
        } else {
            std::cout << "✗ Failed to delete temporary users" << std::endl;
        }
        
        // 9. Connection testing
        std::cout << "\n9. Connection Health Check:" << std::endl;
        
        bool connection_healthy = db_manager->test_connection();
        std::cout << "Connection health: " << (connection_healthy ? "Healthy" : "Unhealthy") << std::endl;
        
        // 10. Cleanup
        std::cout << "\n10. Cleanup:" << std::endl;
        
        // Optionally drop the test table (uncomment if needed)
        // std::string drop_table = "DROP TABLE IF EXISTS users";
        // bool table_dropped = db_manager->drop_query(drop_table);
        // if (table_dropped) {
        //     std::cout << "✓ Test table dropped successfully" << std::endl;
        // }
        
        // Disconnect
        db_manager->disconnect();
        std::cout << "✓ Disconnected from database" << std::endl;
        std::cout << "Connection status: " << (db_manager->is_connected() ? "Connected" : "Disconnected") << std::endl;
        
    } else {
        std::cout << "✗ Failed to connect to database" << std::endl;
        std::cout << "Please ensure:" << std::endl;
        std::cout << "  - PostgreSQL server is running" << std::endl;
        std::cout << "  - Database 'testdb' exists" << std::endl;
        std::cout << "  - User 'testuser' has appropriate permissions" << std::endl;
        std::cout << "  - Connection parameters are correct" << std::endl;
        
        std::cout << "\nTo test with a real database, update the connection string:" << std::endl;
        std::cout << "  host=your_host port=5432 dbname=your_db user=your_user password=your_pass" << std::endl;
    }
    
    std::cout << "\n=== Basic Usage Example completed ===" << std::endl;
    return 0;
}