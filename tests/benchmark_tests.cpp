/**
 * BSD 3-Clause License
 * Copyright (c) 2025, Database System Project
 */

#include <benchmark/benchmark.h>
#include <memory>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <algorithm>

#include "database/database_manager.h"
#include "database/database_types.h"
#include "database/orm/entity.h"
#include "database/monitoring/performance_monitor.h"
#include "database/security/secure_connection.h"
#include "database/async/async_operations.h"

using namespace database;
using namespace database::orm;
using namespace database::monitoring;
using namespace database::security;
using namespace database::async;

// Benchmark database manager operations
static void BM_DatabaseManagerAccess(benchmark::State& state) {
    for (auto _ : state) {
        auto& db = database_manager::handle();
        benchmark::DoNotOptimize(&db);
    }
}
BENCHMARK(BM_DatabaseManagerAccess);

static void BM_DatabaseTypeSettings(benchmark::State& state) {
    auto& db = database_manager::handle();
    for (auto _ : state) {
        db.set_mode(database_types::postgres);
        auto type = db.database_type();
        benchmark::DoNotOptimize(type);
    }
}
BENCHMARK(BM_DatabaseTypeSettings);

static void BM_QueryCreation(benchmark::State& state) {
    auto& db = database_manager::handle();
    db.set_mode(database_types::postgres);

    for (auto _ : state) {
        bool result = db.create_query("SELECT 1");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_QueryCreation);

static void BM_SelectQuery(benchmark::State& state) {
    auto& db = database_manager::handle();
    db.set_mode(database_types::postgres);

    for (auto _ : state) {
        auto result = db.select_query("SELECT 1");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_SelectQuery);

// Benchmark entity for ORM performance tests
class BenchmarkUser : public entity_base
{
    ENTITY_TABLE("benchmark_users")

    ENTITY_FIELD(int64_t, id, primary_key() | auto_increment())
    ENTITY_FIELD(std::string, username, not_null() | unique())
    ENTITY_FIELD(std::string, email, not_null())
    ENTITY_FIELD(bool, is_active, not_null())

    ENTITY_METADATA()

public:
    BenchmarkUser() { is_active = true; }
};

void BenchmarkUser::initialize_metadata() {
    metadata_.add_field(id_field());
    metadata_.add_field(username_field());
    metadata_.add_field(email_field());
    metadata_.add_field(is_active_field());
}

// Phase 4: ORM Framework Benchmarks
static void BM_ORMEntityCreation(benchmark::State& state) {
    for (auto _ : state) {
        BenchmarkUser user;
        user.username = "benchmark_user";
        user.email = "benchmark@test.com";
        benchmark::DoNotOptimize(user);
    }
}
BENCHMARK(BM_ORMEntityCreation);

static void BM_ORMEntityMetadataAccess(benchmark::State& state) {
    BenchmarkUser user;
    for (auto _ : state) {
        const auto& metadata = user.get_metadata();
        benchmark::DoNotOptimize(metadata.table_name());
        benchmark::DoNotOptimize(metadata.fields());
    }
}
BENCHMARK(BM_ORMEntityMetadataAccess);

static void BM_ORMEntityFieldAccess(benchmark::State& state) {
    BenchmarkUser user;
    user.username = "test_user";
    user.email = "test@example.com";

    for (auto _ : state) {
        auto username = user.username.get();
        auto email = user.email.get();
        auto active = user.is_active.get();
        benchmark::DoNotOptimize(username);
        benchmark::DoNotOptimize(email);
        benchmark::DoNotOptimize(active);
    }
}
BENCHMARK(BM_ORMEntityFieldAccess);

static void BM_ORMEntityManager(benchmark::State& state) {
    entity_manager& mgr = entity_manager::instance();
    mgr.register_entity<BenchmarkUser>();

    for (auto _ : state) {
        const auto& metadata = mgr.get_metadata<BenchmarkUser>();
        benchmark::DoNotOptimize(metadata);
    }
}
BENCHMARK(BM_ORMEntityManager);

// Phase 4: Performance Monitoring Benchmarks
static void BM_PerformanceMonitorConfiguration(benchmark::State& state) {
    auto& monitor = performance_monitor::instance();
    monitoring_config config;
    config.enable_query_tracking = true;
    config.enable_connection_tracking = true;

    for (auto _ : state) {
        monitor.configure(config);
        benchmark::DoNotOptimize(&monitor);
    }
}
BENCHMARK(BM_PerformanceMonitorConfiguration);

static void BM_QueryMetricsRecording(benchmark::State& state) {
    auto& monitor = performance_monitor::instance();
    monitoring_config config;
    config.enable_query_tracking = true;
    monitor.configure(config);

    query_metrics metrics;
    metrics.query_type = "SELECT";
    metrics.execution_time = std::chrono::milliseconds(10);
    metrics.success = true;
    metrics.rows_affected = 100;

    for (auto _ : state) {
        monitor.record_query_execution(metrics);
    }
}
BENCHMARK(BM_QueryMetricsRecording);

static void BM_ConnectionMetricsRecording(benchmark::State& state) {
    auto& monitor = performance_monitor::instance();
    monitoring_config config;
    config.enable_connection_tracking = true;
    monitor.configure(config);

    connection_metrics metrics;
    metrics.total_connections.store(20);
    metrics.active_connections.store(10);
    metrics.idle_connections.store(10);

    for (auto _ : state) {
        monitor.record_connection_metrics(metrics);
    }
}
BENCHMARK(BM_ConnectionMetricsRecording);

static void BM_SystemMetricsAccess(benchmark::State& state) {
    auto& monitor = performance_monitor::instance();

    for (auto _ : state) {
        const auto& system_metrics = monitor.get_system_metrics();
        benchmark::DoNotOptimize(system_metrics.cpu_usage_percent);
        benchmark::DoNotOptimize(system_metrics.memory_usage_percent);
    }
}
BENCHMARK(BM_SystemMetricsAccess);

// Phase 4: Security Framework Benchmarks (Conceptual)
static void BM_SecurityConfigurationOverhead(benchmark::State& state) {
    // Mock security configuration benchmark
    struct MockSecurityConfig {
        bool tls_enabled = true;
        std::string cipher_suite = "AES256-GCM-SHA384";
        std::vector<std::string> permissions;
    };

    for (auto _ : state) {
        MockSecurityConfig config;
        config.permissions = {"read", "write", "admin"};

        // Simulate permission check overhead
        bool has_permission = std::find(config.permissions.begin(),
                                       config.permissions.end(), "read") != config.permissions.end();
        benchmark::DoNotOptimize(has_permission);
    }
}
BENCHMARK(BM_SecurityConfigurationOverhead);

static void BM_SecureConnectionHandshake(benchmark::State& state) {
    // Simulate TLS handshake overhead
    for (auto _ : state) {
        // Mock TLS handshake simulation
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        bool handshake_success = true;
        benchmark::DoNotOptimize(handshake_success);
    }
}
BENCHMARK(BM_SecureConnectionHandshake);

static void BM_CredentialValidation(benchmark::State& state) {
    // Mock credential validation benchmark
    std::string username = "test_user";
    std::string password_hash = "hashed_password_123456789";

    for (auto _ : state) {
        // Simulate password hash verification
        bool valid = (username.length() > 0 && password_hash.length() > 10);
        benchmark::DoNotOptimize(valid);
    }
}
BENCHMARK(BM_CredentialValidation);

// Phase 4: Asynchronous Operations Benchmarks
static void BM_AsyncExecutorCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto& executor = async_executor::instance();
        benchmark::DoNotOptimize(&executor);
    }
}
BENCHMARK(BM_AsyncExecutorCreation);

static void BM_AsyncOperationSubmission(benchmark::State& state) {
    auto& executor = async_executor::instance();
    async_config config;
    config.thread_pool_size = 4;
    config.max_concurrent_operations = 100;
    executor.configure(config);

    for (auto _ : state) {
        auto future = executor.execute_async([]() -> int {
            return 42;
        });
        int result = future.get();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_AsyncOperationSubmission);

static void BM_AsyncConnectionPoolAccess(benchmark::State& state) {
    async_connection_pool pool;
    async_pool_config config;
    config.min_connections = 5;
    config.max_connections = 20;
    config.connection_timeout = std::chrono::seconds(1);
    pool.configure(config);

    for (auto _ : state) {
        auto future = pool.get_connection_async();
        auto result = future.get();
        benchmark::DoNotOptimize(result.success);
    }
}
BENCHMARK(BM_AsyncConnectionPoolAccess);

// Concurrent operations benchmark
static void BM_ConcurrentAsyncOperations(benchmark::State& state) {
    auto& executor = async_executor::instance();
    async_config config;
    config.thread_pool_size = 8;
    config.max_concurrent_operations = 200;
    executor.configure(config);

    for (auto _ : state) {
        std::vector<std::future<int>> futures;
        const int num_operations = state.range(0);

        // Submit concurrent operations
        for (int i = 0; i < num_operations; ++i) {
            auto future = executor.execute_async([i]() -> int {
                // Simulate small amount of work
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                return i;
            });
            futures.push_back(std::move(future));
        }

        // Wait for all operations
        for (auto& future : futures) {
            int result = future.get();
            benchmark::DoNotOptimize(result);
        }
    }
}
BENCHMARK(BM_ConcurrentAsyncOperations)->Arg(10)->Arg(50)->Arg(100);

// Phase 4: Connection Pool Benchmarks
static void BM_ConnectionPoolCreation(benchmark::State& state) {
    auto& db = database_manager::handle();

    for (auto _ : state) {
        connection_pool_config config;
        config.connection_string = "test_connection";
        config.min_connections = 5;
        config.max_connections = 20;

        // Note: May fail in test environment, but benchmarks the API call
        db.create_connection_pool(database_types::postgres, config);
        benchmark::DoNotOptimize(&config);
    }
}
BENCHMARK(BM_ConnectionPoolCreation);

static void BM_ConnectionPoolStats(benchmark::State& state) {
    auto& db = database_manager::handle();

    for (auto _ : state) {
        auto stats = db.get_connection_pool_stats();
        benchmark::DoNotOptimize(stats);
    }
}
BENCHMARK(BM_ConnectionPoolStats);

// Phase 4: Query Builder Benchmarks
static void BM_SQLQueryBuilderCreation(benchmark::State& state) {
    auto& db = database_manager::handle();

    for (auto _ : state) {
        auto builder = db.create_query_builder(database_types::postgres);
        benchmark::DoNotOptimize(&builder);
    }
}
BENCHMARK(BM_SQLQueryBuilderCreation);

static void BM_SQLQueryBuilding(benchmark::State& state) {
    auto& db = database_manager::handle();
    auto builder = db.create_query_builder(database_types::postgres);

    for (auto _ : state) {
        builder.select({"id", "name", "email"})
               .from("users")
               .where("active", "=", database_value{true})
               .order_by("name");
        benchmark::DoNotOptimize(&builder);
    }
}
BENCHMARK(BM_SQLQueryBuilding);

// Comprehensive system benchmark
static void BM_IntegratedSystemPerformance(benchmark::State& state) {
    // Setup all Phase 4 systems
    auto& db = database_manager::handle();
    auto& monitor = performance_monitor::instance();
    auto& executor = async_executor::instance();

    // Configure systems
    monitoring_config mon_config;
    mon_config.enable_query_tracking = true;
    monitor.configure(mon_config);

    async_config async_config;
    async_config.thread_pool_size = 4;
    executor.configure(async_config);

    // Mock security setup
    struct MockSecurity {
        bool has_permission(const std::string&, const std::string&) { return true; }
    };
    MockSecurity security;

    for (auto _ : state) {
        // Integrated workflow: Security + Monitoring + Async + ORM
        auto future = executor.execute_async([&]() -> bool {
            // Check permissions
            bool can_access = security.has_permission("test_user", "data.select");

            // Create entity
            BenchmarkUser user;
            user.username = "integrated_user";
            user.email = "integrated@test.com";

            // Record performance metrics
            query_metrics metrics;
            metrics.query_type = "INTEGRATED";
            metrics.execution_time = std::chrono::milliseconds(1);
            metrics.success = true;
            monitor.record_query_execution(metrics);

            return can_access && user.is_active.get();
        });

        bool result = future.get();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_IntegratedSystemPerformance);

BENCHMARK_MAIN();