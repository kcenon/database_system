// BSD 3-Clause License
//
// Copyright (c) 2025, kcenon
// All rights reserved.

/**
 * @file test_configuration.cpp
 * @brief Unit tests for integrated database configuration
 *
 * Tests the configuration system including:
 * - Default values
 * - Builder pattern
 * - Enum types
 * - Struct composition
 */

#include "integrated/core/configuration.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

using namespace database::integrated;

/**
 * @brief Test default configuration values
 */
void test_default_values()
{
	std::cout << "Testing default configuration values...\n";

	unified_db_config config;

	// Database defaults
	assert(config.database.type == backend_type::postgres);
	assert(config.database.connection_string == "host=localhost port=5432 dbname=postgres");
	assert(config.database.enable_ssl == false);
	assert(config.database.enable_prepared_statements == true);

	// Connection pool defaults
	assert(config.connection_pool.pool_name == "default_pool");
	assert(config.connection_pool.min_connections == 2);
	assert(config.connection_pool.max_connections == 10);
	assert(config.connection_pool.connection_timeout == std::chrono::seconds(30));
	assert(config.connection_pool.idle_timeout == std::chrono::seconds(300));
	assert(config.connection_pool.enable_health_checks == true);
	assert(config.connection_pool.enable_priority_queue == false);

	// Thread pool defaults
	assert(config.thread.pool_name == "db_thread_pool");
	assert(config.thread.thread_count == 0); // Auto-detect
	assert(config.thread.max_queue_size == 1000);
	assert(config.thread.enable_priority_scheduling == false);
	assert(config.thread.pool_type == thread_pool_type::standard);

	// Logger defaults
	assert(config.logger.enable_query_logging == false);
	assert(config.logger.enable_connection_logging == true);
	assert(config.logger.log_slow_queries == true);
	assert(config.logger.slow_query_threshold == std::chrono::milliseconds(1000));
	assert(config.logger.min_log_level == db_log_level::info);
	assert(config.logger.enable_file_logging == false);

	// Monitoring defaults
	assert(config.monitoring.enable_metrics == true);
	assert(config.monitoring.enable_profiling == false);
	assert(config.monitoring.enable_health_checks == true);
	assert(config.monitoring.connection_usage_warning_threshold == 0.8);
	assert(config.monitoring.enable_prometheus_export == false);
	assert(config.monitoring.prometheus_port == 9090);

	// Integration flags
	assert(config.enable_common_system_integration == true);
	assert(config.enable_thread_system_integration == true);
	assert(config.enable_monitoring_system_integration == true);

	std::cout << "  ✓ All default values correct\n";
}

/**
 * @brief Test builder pattern methods
 */
void test_builder_pattern()
{
	std::cout << "Testing builder pattern...\n";

	// Test method chaining
	auto config = unified_db_config{}
					  .set_backend(backend_type::postgres, "host=192.168.1.100 dbname=myapp")
					  .set_credentials("admin", "secret123")
					  .set_pool_size(5, 25)
					  .set_pool_name("myapp_pool")
					  .set_log_level(db_log_level::debug)
					  .enable_query_logging(true)
					  .enable_slow_query_logging(true, std::chrono::milliseconds(500))
					  .enable_file_logging(true, "/var/log/myapp")
					  .enable_monitoring(true)
					  .enable_prometheus(true, 9091, "/db_metrics")
					  .set_thread_count(8)
					  .enable_priority_scheduling(true)
					  .enable_ssl(true, "/etc/ssl/cert.pem", "/etc/ssl/key.pem")
					  .set_timeouts(std::chrono::seconds(60), std::chrono::seconds(600));

	// Verify builder results
	assert(config.database.type == backend_type::postgres);
	assert(config.database.connection_string == "host=192.168.1.100 dbname=myapp");
	assert(config.database.username == "admin");
	assert(config.database.password == "secret123");
	assert(config.database.enable_ssl == true);
	assert(config.database.ssl_cert_path == "/etc/ssl/cert.pem");
	assert(config.database.ssl_key_path == "/etc/ssl/key.pem");

	assert(config.connection_pool.min_connections == 5);
	assert(config.connection_pool.max_connections == 25);
	assert(config.connection_pool.pool_name == "myapp_pool");
	assert(config.connection_pool.connection_timeout == std::chrono::seconds(60));
	assert(config.connection_pool.idle_timeout == std::chrono::seconds(600));
	assert(config.connection_pool.enable_priority_queue == true);

	assert(config.thread.thread_count == 8);
	assert(config.thread.enable_priority_scheduling == true);
	assert(config.thread.pool_type == thread_pool_type::typed);

	assert(config.logger.min_log_level == db_log_level::debug);
	assert(config.logger.enable_query_logging == true);
	assert(config.logger.log_slow_queries == true);
	assert(config.logger.slow_query_threshold == std::chrono::milliseconds(500));
	assert(config.logger.enable_file_logging == true);
	assert(config.logger.log_directory == "/var/log/myapp");

	assert(config.monitoring.enable_metrics == true);
	assert(config.monitoring.enable_health_checks == true);
	assert(config.monitoring.enable_prometheus_export == true);
	assert(config.monitoring.prometheus_port == 9091);
	assert(config.monitoring.prometheus_endpoint == "/db_metrics");

	std::cout << "  ✓ Builder pattern works correctly\n";
	std::cout << "  ✓ Method chaining works correctly\n";
}

/**
 * @brief Test enum types
 */
void test_enum_types()
{
	std::cout << "Testing enum types...\n";

	// Test log levels
	db_log_level levels[] = {
		db_log_level::trace, db_log_level::debug,   db_log_level::info,    db_log_level::warning,
		db_log_level::error, db_log_level::critical, db_log_level::fatal,
	};

	// Ensure all enum values are unique
	for (size_t i = 0; i < 7; ++i)
	{
		for (size_t j = i + 1; j < 7; ++j)
		{
			assert(levels[i] != levels[j]);
		}
	}

	// Test backend types
	backend_type backends[] = {
		backend_type::postgres,
		backend_type::sqlite,
		backend_type::mongodb,
		backend_type::redis,
	};

	for (size_t i = 0; i < 4; ++i)
	{
		for (size_t j = i + 1; j < 4; ++j)
		{
			assert(backends[i] != backends[j]);
		}
	}

	// Test thread pool types
	assert(thread_pool_type::standard != thread_pool_type::typed);

	std::cout << "  ✓ All enum types are distinct\n";
}

/**
 * @brief Test struct copy and move semantics
 */
void test_struct_semantics()
{
	std::cout << "Testing struct copy/move semantics...\n";

	// Create original config
	auto original = unified_db_config{}.set_backend(backend_type::postgres, "test_db").set_pool_size(3, 15);

	// Test copy constructor
	unified_db_config copied = original;
	assert(copied.database.type == backend_type::postgres);
	assert(copied.database.connection_string == "test_db");
	assert(copied.connection_pool.min_connections == 3);
	assert(copied.connection_pool.max_connections == 15);

	// Test copy assignment
	unified_db_config assigned;
	assigned = original;
	assert(assigned.database.type == backend_type::postgres);
	assert(assigned.database.connection_string == "test_db");

	// Test move constructor
	unified_db_config moved = std::move(original);
	assert(moved.database.type == backend_type::postgres);
	assert(moved.database.connection_string == "test_db");

	// Test move assignment
	unified_db_config move_assigned;
	move_assigned = std::move(copied);
	assert(move_assigned.database.type == backend_type::postgres);

	std::cout << "  ✓ Copy semantics work correctly\n";
	std::cout << "  ✓ Move semantics work correctly\n";
}

/**
 * @brief Test zero-config scenario
 */
void test_zero_config()
{
	std::cout << "Testing zero-config usage...\n";

	// Should be able to create config with zero configuration
	unified_db_config config;

	// Verify it has sensible defaults
	assert(config.database.type == backend_type::postgres);
	assert(!config.database.connection_string.empty());
	assert(config.connection_pool.min_connections > 0);
	assert(config.connection_pool.max_connections > config.connection_pool.min_connections);
	assert(config.monitoring.enable_metrics == true);
	assert(config.logger.enable_connection_logging == true);

	std::cout << "  ✓ Zero-config creates usable configuration\n";
}

/**
 * @brief Main test runner
 */
int main()
{
	std::cout << "=== Running Integrated Database Configuration Tests ===\n\n";

	try
	{
		test_default_values();
		std::cout << std::endl;

		test_builder_pattern();
		std::cout << std::endl;

		test_enum_types();
		std::cout << std::endl;

		test_struct_semantics();
		std::cout << std::endl;

		test_zero_config();
		std::cout << std::endl;

		std::cout << "=== All tests passed! ✓ ===\n";
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Test failed with exception: " << e.what() << "\n";
		return 1;
	}
	catch (...)
	{
		std::cerr << "Test failed with unknown exception\n";
		return 1;
	}
}
