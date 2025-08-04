/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Database System Project
 */

#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include "../postgres_manager.h"

using namespace database_module;

int main() {
    std::cout << "=== Database System - PostgreSQL Advanced Features Example ===" << std::endl;
    
    // 1. PostgreSQL-specific manager creation
    std::cout << "\n1. PostgreSQL Manager Setup:" << std::endl;
    
    auto pg_manager = std::make_shared<postgres_manager>();
    
    // Connection string for PostgreSQL
    std::string connection_string = "host=localhost port=5432 dbname=testdb user=testuser password=testpass";
    std::cout << "PostgreSQL manager created" << std::endl;
    std::cout << "Note: This example demonstrates PostgreSQL-specific features" << std::endl;
    
    // 2. Connection and advanced table creation
    std::cout << "\n2. Advanced Table Creation:" << std::endl;
    
    bool connected = pg_manager->connect(connection_string);
    
    if (connected) {
        std::cout << "✓ Connected to PostgreSQL database" << std::endl;
        
        // Create table with PostgreSQL-specific features
        std::string create_advanced_table = R"(
            CREATE TABLE IF NOT EXISTS products (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                description TEXT,
                price DECIMAL(10,2) CHECK (price >= 0),
                category_id INTEGER,
                tags TEXT[],
                metadata JSONB,
                search_vector TSVECTOR,
                created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
            )
        )";
        
        std::cout << "Creating advanced products table with PostgreSQL features..." << std::endl;
        bool table_created = pg_manager->create_query(create_advanced_table);
        
        if (table_created) {
            std::cout << "✓ Advanced products table created successfully" << std::endl;
            
            // Create indexes
            std::vector<std::string> index_queries = {
                "CREATE INDEX IF NOT EXISTS idx_products_name ON products USING btree(name)",
                "CREATE INDEX IF NOT EXISTS idx_products_category ON products(category_id)",
                "CREATE INDEX IF NOT EXISTS idx_products_tags ON products USING gin(tags)",
                "CREATE INDEX IF NOT EXISTS idx_products_metadata ON products USING gin(metadata)",
                "CREATE INDEX IF NOT EXISTS idx_products_search ON products USING gin(search_vector)"
            };
            
            for (const auto& idx_query : index_queries) {
                if (pg_manager->create_query(idx_query)) {
                    std::cout << "✓ Index created" << std::endl;
                }
            }
        }
        
        // 3. Array operations
        std::cout << "\n3. PostgreSQL Array Operations:" << std::endl;
        
        std::vector<std::string> product_inserts = {
            R"(INSERT INTO products (name, description, price, category_id, tags, metadata) 
               VALUES ('Gaming Laptop', 'High-performance gaming laptop', 1299.99, 1, 
                       ARRAY['gaming', 'laptop', 'computer'], 
                       '{"brand": "TechCorp", "warranty": "2 years", "specs": {"ram": "16GB", "storage": "1TB SSD"}}'::jsonb))",
            
            R"(INSERT INTO products (name, description, price, category_id, tags, metadata) 
               VALUES ('Wireless Mouse', 'Ergonomic wireless mouse', 29.99, 2, 
                       ARRAY['mouse', 'wireless', 'accessory'], 
                       '{"brand": "MouseCorp", "warranty": "1 year", "specs": {"dpi": 3200, "battery": "AA"}}'::jsonb))",
            
            R"(INSERT INTO products (name, description, price, category_id, tags, metadata) 
               VALUES ('Mechanical Keyboard', 'RGB mechanical gaming keyboard', 149.99, 2, 
                       ARRAY['keyboard', 'mechanical', 'gaming', 'rgb'], 
                       '{"brand": "KeyCorp", "warranty": "3 years", "specs": {"switches": "Cherry MX", "backlight": "RGB"}}'::jsonb))"
        };
        
        for (const auto& insert_query : product_inserts) {
            bool inserted = pg_manager->insert_query(insert_query);
            if (inserted) {
                std::cout << "✓ Product with arrays and JSON inserted" << std::endl;
            }
        }
        
        // Query products with array operations
        std::cout << "\nQuerying products with array operations:" << std::endl;
        
        // Find products with specific tag
        std::string array_query1 = "SELECT name, tags FROM products WHERE 'gaming' = ANY(tags)";
        auto gaming_products = pg_manager->select_query(array_query1);
        if (gaming_products) {
            std::cout << "Products with 'gaming' tag:" << std::endl;
            std::cout << *gaming_products << std::endl;
        }
        
        // Find products with multiple tags
        std::string array_query2 = "SELECT name, tags FROM products WHERE tags && ARRAY['laptop', 'computer']";
        auto tech_products = pg_manager->select_query(array_query2);
        if (tech_products) {
            std::cout << "Products with laptop/computer tags:" << std::endl;
            std::cout << *tech_products << std::endl;
        }
        
        // 4. JSONB operations
        std::cout << "\n4. PostgreSQL JSONB Operations:" << std::endl;
        
        // Query by JSON field
        std::string json_query1 = "SELECT name, metadata->>'brand' as brand FROM products WHERE metadata->>'brand' = 'TechCorp'";
        auto techcorp_products = pg_manager->select_query(json_query1);
        if (techcorp_products) {
            std::cout << "TechCorp products:" << std::endl;
            std::cout << *techcorp_products << std::endl;
        }
        
        // Query nested JSON
        std::string json_query2 = "SELECT name, metadata->'specs'->>'ram' as ram FROM products WHERE metadata->'specs'->>'ram' IS NOT NULL";
        auto products_with_ram = pg_manager->select_query(json_query2);
        if (products_with_ram) {
            std::cout << "Products with RAM specifications:" << std::endl;
            std::cout << *products_with_ram << std::endl;
        }
        
        // JSON containment query
        std::string json_query3 = "SELECT name, metadata FROM products WHERE metadata @> '{\"warranty\": \"2 years\"}'";
        auto warranty_products = pg_manager->select_query(json_query3);
        if (warranty_products) {
            std::cout << "Products with 2-year warranty:" << std::endl;
            std::cout << *warranty_products << std::endl;
        }
        
        // 5. Common Table Expressions (CTEs)
        std::cout << "\n5. Common Table Expressions (CTEs):" << std::endl;
        
        std::string cte_query = R"(
            WITH product_stats AS (
                SELECT 
                    category_id,
                    COUNT(*) as product_count,
                    AVG(price) as avg_price,
                    MIN(price) as min_price,
                    MAX(price) as max_price
                FROM products 
                GROUP BY category_id
            ),
            expensive_products AS (
                SELECT name, price, category_id
                FROM products 
                WHERE price > 100
            )
            SELECT 
                ps.category_id,
                ps.product_count,
                ROUND(ps.avg_price, 2) as avg_price,
                ps.min_price,
                ps.max_price,
                STRING_AGG(ep.name, ', ') as expensive_products
            FROM product_stats ps
            LEFT JOIN expensive_products ep ON ps.category_id = ep.category_id
            GROUP BY ps.category_id, ps.product_count, ps.avg_price, ps.min_price, ps.max_price
            ORDER BY ps.category_id
        )";
        
        auto cte_result = pg_manager->select_query(cte_query);
        if (cte_result) {
            std::cout << "Product statistics using CTE:" << std::endl;
            std::cout << *cte_result << std::endl;
        }
        
        // 6. Full-text search setup
        std::cout << "\n6. Full-Text Search:" << std::endl;
        
        // Update search vectors
        std::string update_search_vector = R"(
            UPDATE products 
            SET search_vector = to_tsvector('english', name || ' ' || COALESCE(description, ''))
        )";
        
        if (pg_manager->update_query(update_search_vector)) {
            std::cout << "✓ Search vectors updated" << std::endl;
            
            // Perform full-text search
            std::string search_query = R"(
                SELECT name, description, ts_rank(search_vector, query) as rank
                FROM products, plainto_tsquery('english', 'gaming laptop') query
                WHERE search_vector @@ query
                ORDER BY rank DESC
            )";
            
            auto search_results = pg_manager->select_query(search_query);
            if (search_results) {
                std::cout << "Full-text search results for 'gaming laptop':" << std::endl;
                std::cout << *search_results << std::endl;
            }
        }
        
        // 7. Window functions
        std::cout << "\n7. Window Functions:" << std::endl;
        
        std::string window_query = R"(
            SELECT 
                name,
                price,
                category_id,
                ROW_NUMBER() OVER (PARTITION BY category_id ORDER BY price DESC) as price_rank,
                RANK() OVER (ORDER BY price DESC) as overall_price_rank,
                LAG(price) OVER (PARTITION BY category_id ORDER BY price) as prev_price,
                LEAD(price) OVER (PARTITION BY category_id ORDER BY price) as next_price,
                AVG(price) OVER (PARTITION BY category_id) as category_avg_price
            FROM products
            ORDER BY category_id, price DESC
        )";
        
        auto window_results = pg_manager->select_query(window_query);
        if (window_results) {
            std::cout << "Window function results:" << std::endl;
            std::cout << *window_results << std::endl;
        }
        
        // 8. Prepared statements (if implemented)
        std::cout << "\n8. Prepared Statements:" << std::endl;
        
        // Note: This demonstrates the API, actual implementation depends on postgres_manager
        std::cout << "Creating prepared statement for product search..." << std::endl;
        std::string prep_stmt = "SELECT name, price FROM products WHERE price BETWEEN $1 AND $2 ORDER BY price";
        
        // This would require implementation in postgres_manager
        // bool stmt_created = pg_manager->create_prepared_statement("search_by_price", prep_stmt);
        // if (stmt_created) {
        //     std::cout << "✓ Prepared statement created" << std::endl;
        //     
        //     std::vector<std::string> params = {"50.00", "200.00"};
        //     bool executed = pg_manager->execute_prepared("search_by_price", params);
        //     if (executed) {
        //         std::cout << "✓ Prepared statement executed" << std::endl;
        //     }
        // }
        
        std::cout << "Note: Prepared statement support requires additional implementation" << std::endl;
        
        // 9. Transaction with savepoints
        std::cout << "\n9. Advanced Transaction Management:" << std::endl;
        
        bool transaction_started = pg_manager->begin_transaction();
        if (transaction_started) {
            std::cout << "✓ Transaction started" << std::endl;
            
            // Insert a test product
            std::string test_insert = R"(
                INSERT INTO products (name, description, price, category_id, tags, metadata) 
                VALUES ('Test Product', 'This is a test product', 99.99, 3, 
                        ARRAY['test'], '{"test": true}'::jsonb)
            )";
            
            bool test_inserted = pg_manager->insert_query(test_insert);
            if (test_inserted) {
                std::cout << "✓ Test product inserted in transaction" << std::endl;
                
                // Rollback instead of commit for demonstration
                std::cout << "Demonstrating rollback..." << std::endl;
                bool rolled_back = pg_manager->rollback_transaction();
                if (rolled_back) {
                    std::cout << "✓ Transaction rolled back - test product not saved" << std::endl;
                }
            }
        }
        
        // 10. Final verification
        std::cout << "\n10. Final Verification:" << std::endl;
        
        std::string final_count = "SELECT COUNT(*) as total_products FROM products";
        auto count_result = pg_manager->select_query(final_count);
        if (count_result) {
            std::cout << "Total products in database: " << *count_result << std::endl;
        }
        
        // Clean up (optional)
        std::cout << "\nOptional cleanup (uncomment to remove test data):" << std::endl;
        std::cout << "-- DELETE FROM products; -- Remove test products" << std::endl;
        std::cout << "-- DROP TABLE products; -- Remove test table" << std::endl;
        
        // Disconnect
        pg_manager->disconnect();
        std::cout << "✓ Disconnected from PostgreSQL database" << std::endl;
        
    } else {
        std::cout << "✗ Failed to connect to PostgreSQL database" << std::endl;
        std::cout << "Please ensure PostgreSQL server is running and connection parameters are correct" << std::endl;
    }
    
    std::cout << "\n=== PostgreSQL Advanced Features Example completed ===" << std::endl;
    return 0;
}