/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Database System Project
 */

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include "../database_manager.h"

using namespace database_module;

class connection_pool_demo {
public:
    void run_demo() {
        std::cout << "=== Database System - Connection Pool Demo ===" << std::endl;
        
        demo_single_connection();
        demo_multiple_connections();
        demo_concurrent_access();
        demo_connection_resilience();
        
        std::cout << "\n=== Connection Pool Demo completed ===" << std::endl;
    }

private:
    void demo_single_connection() {
        std::cout << "\n1. Single Connection Demo:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        auto db_manager = std::make_shared<database_manager>();
        db_manager->set_database_type(database_types::postgres);
        
        std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
        
        std::cout << "Connecting to database..." << std::endl;
        bool connected = db_manager->connect(connection_string);
        
        if (connected) {
            std::cout << "✓ Single connection established" << std::endl;
            std::cout << "Connection status: " << (db_manager->is_connected() ? "Connected" : "Disconnected") << std::endl;
            
            // Create test table
            setup_test_table(db_manager);
            
            // Perform basic operations
            perform_basic_operations(db_manager, 1);
            
            db_manager->disconnect();
            std::cout << "✓ Connection closed" << std::endl;
        } else {
            std::cout << "✗ Failed to establish single connection" << std::endl;
            std::cout << "Note: This demo requires a running PostgreSQL server" << std::endl;
        }
    }
    
    void demo_multiple_connections() {
        std::cout << "\n2. Multiple Connections Demo:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        const int num_connections = 5;
        std::vector<std::shared_ptr<database_manager>> connections;
        std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
        
        std::cout << "Creating " << num_connections << " database connections..." << std::endl;
        
        // Create multiple connections
        for (int i = 0; i < num_connections; ++i) {
            auto db_manager = std::make_shared<database_manager>();
            db_manager->set_database_type(database_types::postgres);
            
            bool connected = db_manager->connect(connection_string);
            if (connected) {
                connections.push_back(db_manager);
                std::cout << "✓ Connection " << (i + 1) << " established" << std::endl;
            } else {
                std::cout << "✗ Failed to establish connection " << (i + 1) << std::endl;
            }
        }
        
        std::cout << "Successfully created " << connections.size() << " connections" << std::endl;
        
        if (!connections.empty()) {
            // Use different connections for different operations
            for (size_t i = 0; i < connections.size(); ++i) {
                std::cout << "Using connection " << (i + 1) << ":" << std::endl;
                perform_basic_operations(connections[i], i + 1);
            }
            
            // Close all connections
            for (auto& conn : connections) {
                conn->disconnect();
            }
            std::cout << "✓ All connections closed" << std::endl;
        }
    }
    
    void demo_concurrent_access() {
        std::cout << "\n3. Concurrent Access Demo:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        const int num_threads = 4;
        const int operations_per_thread = 50;
        std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
        
        std::cout << "Starting " << num_threads << " concurrent threads..." << std::endl;
        std::cout << "Each thread will perform " << operations_per_thread << " operations" << std::endl;
        
        std::atomic<int> successful_connections{0};
        std::atomic<int> total_operations{0};
        std::atomic<int> successful_operations{0};
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> workers;
        
        // Create worker threads
        for (int t = 0; t < num_threads; ++t) {
            workers.emplace_back([&, t, connection_string, operations_per_thread]() {
                auto db_manager = std::make_shared<database_manager>();
                db_manager->set_database_type(database_types::postgres);
                
                bool connected = db_manager->connect(connection_string);
                if (connected) {
                    successful_connections.fetch_add(1);
                    
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(1, 1000);
                    
                    for (int op = 0; op < operations_per_thread; ++op) {
                        total_operations.fetch_add(1);
                        
                        // Simulate different types of database operations
                        int operation_type = op % 4;
                        bool operation_success = false;
                        
                        switch (operation_type) {
                            case 0: { // Insert
                                std::string insert_query = "INSERT INTO connection_test (thread_id, operation_id, data, timestamp) VALUES (" +
                                    std::to_string(t) + ", " + std::to_string(op) + ", 'data_" + std::to_string(dis(gen)) + "', CURRENT_TIMESTAMP)";
                                operation_success = db_manager->insert_query(insert_query);
                                break;
                            }
                            case 1: { // Select
                                std::string select_query = "SELECT COUNT(*) FROM connection_test WHERE thread_id = " + std::to_string(t);
                                auto result = db_manager->select_query(select_query);
                                operation_success = (result != std::nullopt);
                                break;
                            }
                            case 2: { // Update
                                std::string update_query = "UPDATE connection_test SET data = 'updated_" + std::to_string(dis(gen)) + 
                                    "' WHERE thread_id = " + std::to_string(t) + " AND operation_id = " + std::to_string(op % 10);
                                operation_success = db_manager->update_query(update_query);
                                break;
                            }
                            case 3: { // Connection health check
                                operation_success = db_manager->test_connection();
                                break;
                            }
                        }
                        
                        if (operation_success) {
                            successful_operations.fetch_add(1);
                        }
                        
                        // Small delay to simulate real workload
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    
                    db_manager->disconnect();
                } else {
                    std::cout << "Thread " << t << " failed to connect" << std::endl;
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& worker : workers) {
            worker.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\nConcurrent access results:" << std::endl;
        std::cout << "  Successful connections: " << successful_connections.load() << "/" << num_threads << std::endl;
        std::cout << "  Total operations attempted: " << total_operations.load() << std::endl;
        std::cout << "  Successful operations: " << successful_operations.load() << std::endl;
        std::cout << "  Success rate: " << std::fixed << std::setprecision(2) 
                  << (double)successful_operations.load() / total_operations.load() * 100 << "%" << std::endl;
        std::cout << "  Total time: " << duration.count() << " ms" << std::endl;
        std::cout << "  Operations per second: " << std::fixed << std::setprecision(2)
                  << (double)successful_operations.load() / duration.count() * 1000 << std::endl;
    }
    
    void demo_connection_resilience() {
        std::cout << "\n4. Connection Resilience Demo:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        auto db_manager = std::make_shared<database_manager>();
        db_manager->set_database_type(database_types::postgres);
        
        std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
        
        std::cout << "Testing connection resilience and recovery..." << std::endl;
        
        bool connected = db_manager->connect(connection_string);
        if (connected) {
            std::cout << "✓ Initial connection established" << std::endl;
            
            // Test connection health monitoring
            std::cout << "\nTesting connection health monitoring:" << std::endl;
            for (int i = 0; i < 5; ++i) {
                bool healthy = db_manager->test_connection();
                std::cout << "Health check " << (i + 1) << ": " << (healthy ? "Healthy" : "Unhealthy") << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Test reconnection capability
            std::cout << "\nTesting reconnection capability:" << std::endl;
            db_manager->disconnect();
            std::cout << "Connection closed deliberately" << std::endl;
            std::cout << "Connection status: " << (db_manager->is_connected() ? "Connected" : "Disconnected") << std::endl;
            
            // Attempt to reconnect
            std::cout << "Attempting to reconnect..." << std::endl;
            bool reconnected = db_manager->reconnect();
            if (reconnected) {
                std::cout << "✓ Reconnection successful" << std::endl;
                std::cout << "Connection status: " << (db_manager->is_connected() ? "Connected" : "Disconnected") << std::endl;
            } else {
                std::cout << "✗ Reconnection failed" << std::endl;
            }
            
            // Test connection under load
            std::cout << "\nTesting connection under sustained load:" << std::endl;
            const int load_operations = 100;
            int successful_ops = 0;
            
            auto load_start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < load_operations; ++i) {
                std::string query = "SELECT " + std::to_string(i) + " as operation_number, CURRENT_TIMESTAMP as timestamp";
                auto result = db_manager->select_query(query);
                if (result) {
                    successful_ops++;
                }
                
                // Brief pause between operations
                if (i % 10 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            
            auto load_end = std::chrono::high_resolution_clock::now();
            auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start);
            
            std::cout << "Load test results:" << std::endl;
            std::cout << "  Operations completed: " << successful_ops << "/" << load_operations << std::endl;
            std::cout << "  Success rate: " << std::fixed << std::setprecision(2) 
                      << (double)successful_ops / load_operations * 100 << "%" << std::endl;
            std::cout << "  Duration: " << load_duration.count() << " ms" << std::endl;
            std::cout << "  Operations per second: " << std::fixed << std::setprecision(2)
                      << (double)successful_ops / load_duration.count() * 1000 << std::endl;
            
            // Final health check
            bool final_health = db_manager->test_connection();
            std::cout << "Final health check: " << (final_health ? "Healthy" : "Unhealthy") << std::endl;
            
            db_manager->disconnect();
            std::cout << "✓ Connection closed cleanly" << std::endl;
            
        } else {
            std::cout << "✗ Failed to establish initial connection for resilience testing" << std::endl;
        }
    }
    
    void setup_test_table(std::shared_ptr<database_manager> db_manager) {
        std::string create_table = R"(
            CREATE TABLE IF NOT EXISTS connection_test (
                id SERIAL PRIMARY KEY,
                thread_id INTEGER,
                operation_id INTEGER,
                data VARCHAR(255),
                timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        bool created = db_manager->create_query(create_table);
        if (created) {
            std::cout << "✓ Test table ready" << std::endl;
        } else {
            std::cout << "Test table creation skipped (may already exist)" << std::endl;
        }
    }
    
    void perform_basic_operations(std::shared_ptr<database_manager> db_manager, int connection_id) {
        // Insert test data
        std::string insert_query = "INSERT INTO connection_test (thread_id, operation_id, data) VALUES (" +
            std::to_string(connection_id) + ", 1, 'test_data_" + std::to_string(connection_id) + "')";
        
        bool inserted = db_manager->insert_query(insert_query);
        if (inserted) {
            std::cout << "  ✓ Insert operation successful" << std::endl;
        }
        
        // Select test data
        std::string select_query = "SELECT COUNT(*) FROM connection_test WHERE thread_id = " + std::to_string(connection_id);
        auto result = db_manager->select_query(select_query);
        if (result) {
            std::cout << "  ✓ Select operation successful: " << *result << std::endl;
        }
        
        // Update test data
        std::string update_query = "UPDATE connection_test SET data = 'updated_data_" + 
            std::to_string(connection_id) + "' WHERE thread_id = " + std::to_string(connection_id);
        
        bool updated = db_manager->update_query(update_query);
        if (updated) {
            std::cout << "  ✓ Update operation successful" << std::endl;
        }
    }
};

int main() {
    connection_pool_demo demo;
    demo.run_demo();
    return 0;
}