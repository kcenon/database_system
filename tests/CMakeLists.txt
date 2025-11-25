##################################################
# Database System Tests Configuration
##################################################

# Enable testing
enable_testing()

# Find test dependencies
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    find_package(gtest QUIET)
endif()
find_package(benchmark QUIET)

##################################################
# Unit Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    # Create unit test executable
    add_executable(database_unit_tests
        unit_tests.cpp
    )

    # Link dependencies - handle different target names
    if(GTest_FOUND)
        target_link_libraries(database_unit_tests PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(database_unit_tests PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()
    
    # Set properties
    set_target_properties(database_unit_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # Add test
    add_test(NAME DatabaseUnitTests COMMAND database_unit_tests)

    # Enable test discovery
    include(GoogleTest)

    # Sanitizer builds need longer timeout (5-10x slowdown for most tests)
    # Note: Some tests are skipped under sanitizers (see test source for GTEST_SKIP)
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
        set(TEST_TIMEOUT 300)  # 5 minutes per test
    else()
        set(TEST_TIMEOUT 60)   # 1 minute per test
    endif()

    gtest_discover_tests(database_unit_tests
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )
else()
    message(WARNING "GTest not found - unit tests will not be built")
endif()

##################################################
# Benchmark Tests
##################################################

if(benchmark_FOUND)
    # Create benchmark executable
    add_executable(database_benchmark_tests
        benchmark_tests.cpp
    )
    
    # Link dependencies
    target_link_libraries(database_benchmark_tests PRIVATE
        database
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
    )
    
    # Set properties
    set_target_properties(database_benchmark_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # Platform-specific optimizations
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(database_benchmark_tests PRIVATE
            -O3
            -march=native
        )
    elseif(MSVC)
        target_compile_options(database_benchmark_tests PRIVATE
            /O2
        )
    endif()
else()
    message(WARNING "Google Benchmark not found - benchmark tests will not be built")
endif()

##################################################
# MySQL Backend Tests (DB-001)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(mysql_manager_test
        mysql_manager_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(mysql_manager_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(mysql_manager_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(mysql_manager_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME MySQLManagerTests COMMAND mysql_manager_test)

    gtest_discover_tests(mysql_manager_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "MySQL manager tests configured (DB-001)")
endif()

##################################################
# SQLite Backend Tests (DB-001)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(sqlite_manager_test
        sqlite_manager_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(sqlite_manager_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(sqlite_manager_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(sqlite_manager_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME SQLiteManagerTests COMMAND sqlite_manager_test)

    gtest_discover_tests(sqlite_manager_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "SQLite manager tests configured (DB-001)")
endif()

##################################################
# Resilience Module Tests (DB-003)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(resilience_test
        resilience_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(resilience_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(resilience_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(resilience_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME ResilienceModuleTests COMMAND resilience_test)

    gtest_discover_tests(resilience_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Resilience module tests configured (DB-003)")
endif()

##################################################
# Thread Safety Tests (Phase 1 - Task 1.5)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(database_thread_safety_test
        thread_safety_tests.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(database_thread_safety_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(database_thread_safety_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(database_thread_safety_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME DatabaseThreadSafetyTests COMMAND database_thread_safety_test)

    gtest_discover_tests(database_thread_safety_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )
endif()

##################################################
# Integrated Database Configuration Tests
##################################################

if(BUILD_INTEGRATED_DATABASE)
    # Simple configuration test (no external dependencies)
    add_executable(integrated_configuration_test
        integrated/test_configuration.cpp
    )

    target_link_libraries(integrated_configuration_test PRIVATE
        integrated_database
    )

    set_target_properties(integrated_configuration_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedConfigurationTest COMMAND integrated_configuration_test)

    # Logger adapter test
    add_executable(integrated_logger_adapter_test
        integrated/test_logger_adapter.cpp
    )

    target_link_libraries(integrated_logger_adapter_test PRIVATE
        integrated_database
        Threads::Threads
    )

    set_target_properties(integrated_logger_adapter_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedLoggerAdapterTest COMMAND integrated_logger_adapter_test)

    # Monitoring adapter test
    add_executable(integrated_monitoring_adapter_test
        integrated/test_monitoring_adapter.cpp
    )

    target_link_libraries(integrated_monitoring_adapter_test PRIVATE
        integrated_database
        Threads::Threads
    )

    set_target_properties(integrated_monitoring_adapter_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedMonitoringAdapterTest COMMAND integrated_monitoring_adapter_test)

    # Thread adapter test
    add_executable(integrated_thread_adapter_test
        integrated/test_thread_adapter.cpp
    )

    target_link_libraries(integrated_thread_adapter_test PRIVATE
        integrated_database
        Threads::Threads
    )

    set_target_properties(integrated_thread_adapter_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedThreadAdapterTest COMMAND integrated_thread_adapter_test)

    # Database coordinator test
    add_executable(integrated_coordinator_test
        integrated/test_database_coordinator.cpp
    )

    target_link_libraries(integrated_coordinator_test PRIVATE
        integrated_database
        Threads::Threads
    )

    set_target_properties(integrated_coordinator_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedCoordinatorTest COMMAND integrated_coordinator_test)

    # Unified database system test (Phase 6)
    add_executable(integrated_unified_system_test
        integrated/test_unified_database_system.cpp
    )

    target_link_libraries(integrated_unified_system_test PRIVATE
        integrated_database
        database
        Threads::Threads
    )

    set_target_properties(integrated_unified_system_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedUnifiedSystemTest COMMAND integrated_unified_system_test)

    message(STATUS "Integrated database configuration test configured")
    message(STATUS "Integrated database logger adapter test configured")
    message(STATUS "Integrated database monitoring adapter test configured")
    message(STATUS "Integrated database thread adapter test configured")
    message(STATUS "Integrated database coordinator test configured")
    message(STATUS "Integrated database unified system test configured (Phase 6)")
endif()

##################################################
# Query Builder Tests (DB-002)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    # SQL Query Builder Tests
    add_executable(sql_query_builder_test
        sql_query_builder_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(sql_query_builder_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(sql_query_builder_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(sql_query_builder_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME SQLQueryBuilderTests COMMAND sql_query_builder_test)

    gtest_discover_tests(sql_query_builder_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    # MongoDB Query Builder Tests
    add_executable(mongodb_query_builder_test
        mongodb_query_builder_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(mongodb_query_builder_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(mongodb_query_builder_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(mongodb_query_builder_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME MongoDBQueryBuilderTests COMMAND mongodb_query_builder_test)

    gtest_discover_tests(mongodb_query_builder_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    # Redis Query Builder Tests
    add_executable(redis_query_builder_test
        redis_query_builder_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(redis_query_builder_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(redis_query_builder_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(redis_query_builder_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME RedisQueryBuilderTests COMMAND redis_query_builder_test)

    gtest_discover_tests(redis_query_builder_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    # Universal Query Builder Tests
    add_executable(universal_query_builder_test
        universal_query_builder_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(universal_query_builder_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(universal_query_builder_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(universal_query_builder_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME UniversalQueryBuilderTests COMMAND universal_query_builder_test)

    gtest_discover_tests(universal_query_builder_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Query builder tests configured (DB-002)")
endif()

##################################################
# Container Protocol Tests (Phase 1 POC Validation)
##################################################

if(USE_CONTAINER_SYSTEM AND (GTest_FOUND OR gtest_FOUND))
    add_executable(integrated_container_protocol_test
        integrated/test_container_protocol.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(integrated_container_protocol_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(integrated_container_protocol_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    target_compile_definitions(integrated_container_protocol_test PRIVATE
        USE_CONTAINER_SYSTEM
    )

    set_target_properties(integrated_container_protocol_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedContainerProtocolTest
             COMMAND integrated_container_protocol_test)

    gtest_discover_tests(integrated_container_protocol_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Container protocol test configured (Phase 1 POC)")
endif()

##################################################
# Stress Tests (DB-009)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    # Async Stress Tests
    add_executable(async_stress_test
        stress/async_stress_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(async_stress_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(async_stress_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(async_stress_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME AsyncStressTests COMMAND async_stress_test)

    # Stress tests need longer timeout
    if(CMAKE_CXX_FLAGS MATCHES "fsanitize")
        set(STRESS_TEST_TIMEOUT 600)
    else()
        set(STRESS_TEST_TIMEOUT 300)
    endif()

    gtest_discover_tests(async_stress_test
        PROPERTIES
            TIMEOUT ${STRESS_TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    # Connection Stress Tests
    add_executable(connection_stress_test
        stress/connection_stress_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(connection_stress_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(connection_stress_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(connection_stress_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME ConnectionStressTests COMMAND connection_stress_test)

    gtest_discover_tests(connection_stress_test
        PROPERTIES
            TIMEOUT ${STRESS_TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    # Memory Stress Tests
    add_executable(memory_stress_test
        stress/memory_stress_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(memory_stress_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(memory_stress_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(memory_stress_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME MemoryStressTests COMMAND memory_stress_test)

    gtest_discover_tests(memory_stress_test
        PROPERTIES
            TIMEOUT ${STRESS_TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Stress tests configured (DB-009)")
endif()

##################################################
# Security Tests (DB-008)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    # SQL Injection Prevention Tests
    add_executable(sql_injection_test
        security/sql_injection_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(sql_injection_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(sql_injection_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(sql_injection_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME SQLInjectionTests COMMAND sql_injection_test)

    gtest_discover_tests(sql_injection_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    # Credential Security Tests
    add_executable(credential_test
        security/credential_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(credential_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(credential_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(credential_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME CredentialSecurityTests COMMAND credential_test)

    gtest_discover_tests(credential_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    # Data Masking Tests
    add_executable(data_masking_test
        security/data_masking_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(data_masking_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(data_masking_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(data_masking_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME DataMaskingTests COMMAND data_masking_test)

    gtest_discover_tests(data_masking_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Security tests configured (DB-008)")
endif()

##################################################
# Test Data Setup
##################################################

# Copy test data files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()

##################################################
# Gateway Tests (DB-004)
##################################################

if((GTest_FOUND OR gtest_FOUND) AND USE_NETWORK_SYSTEM AND ENABLE_REMOTE_DATABASE_GATEWAY)
    add_executable(gateway_test
        gateway_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(gateway_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            Threads::Threads
        )
    else()
        target_link_libraries(gateway_test PRIVATE
            database
            gtest
            gtest_main
            gmock
            Threads::Threads
        )
    endif()

    set_target_properties(gateway_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME GatewayTests COMMAND gateway_test)

    gtest_discover_tests(gateway_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Gateway tests configured (DB-004)")
endif()

##################################################
# Replication Manager Tests (DB-005)
##################################################

if((GTest_FOUND OR gtest_FOUND) AND USE_NETWORK_SYSTEM AND ENABLE_REMOTE_DATABASE_GATEWAY)
    add_executable(replication_test
        replication_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(replication_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            GTest::gmock
            Threads::Threads
        )
    else()
        target_link_libraries(replication_test PRIVATE
            database
            gtest
            gtest_main
            gmock
            Threads::Threads
        )
    endif()

    set_target_properties(replication_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME ReplicationManagerTests COMMAND replication_test)

    gtest_discover_tests(replication_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Replication manager tests configured (DB-005)")
endif()

##################################################
# Distributed Multi-Node Tests (DB-011)
##################################################

# Find CURL for Toxiproxy integration
find_package(CURL QUIET)

if((GTest_FOUND OR gtest_FOUND) AND USE_NETWORK_SYSTEM AND ENABLE_REMOTE_DATABASE_GATEWAY AND CURL_FOUND)
    # Multi-Node Test Framework
    add_library(multi_node_test_framework STATIC
        distributed/multi_node_test_framework.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(multi_node_test_framework PUBLIC
            database
            GTest::gtest
            CURL::libcurl
            Threads::Threads
        )
    else()
        target_link_libraries(multi_node_test_framework PUBLIC
            database
            gtest
            CURL::libcurl
            Threads::Threads
        )
    endif()

    target_include_directories(multi_node_test_framework PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    set_target_properties(multi_node_test_framework PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Cluster Formation Tests
    add_executable(cluster_formation_test
        distributed/cluster_formation_test.cpp
    )

    target_link_libraries(cluster_formation_test PRIVATE
        multi_node_test_framework
        database
    )

    if(GTest_FOUND)
        target_link_libraries(cluster_formation_test PRIVATE
            GTest::gtest
            GTest::gtest_main
        )
    else()
        target_link_libraries(cluster_formation_test PRIVATE
            gtest
            gtest_main
        )
    endif()

    set_target_properties(cluster_formation_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME ClusterFormationTests COMMAND cluster_formation_test)

    gtest_discover_tests(cluster_formation_test
        PROPERTIES
            TIMEOUT 300
        DISCOVERY_TIMEOUT 60
    )

    # Failover Tests
    add_executable(failover_test
        distributed/failover_test.cpp
    )

    target_link_libraries(failover_test PRIVATE
        multi_node_test_framework
        database
    )

    if(GTest_FOUND)
        target_link_libraries(failover_test PRIVATE
            GTest::gtest
            GTest::gtest_main
        )
    else()
        target_link_libraries(failover_test PRIVATE
            gtest
            gtest_main
        )
    endif()

    set_target_properties(failover_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME FailoverTests COMMAND failover_test)

    gtest_discover_tests(failover_test
        PROPERTIES
            TIMEOUT 300
        DISCOVERY_TIMEOUT 60
    )

    # Network Partition Tests
    add_executable(network_partition_test
        distributed/network_partition_test.cpp
    )

    target_link_libraries(network_partition_test PRIVATE
        multi_node_test_framework
        database
    )

    if(GTest_FOUND)
        target_link_libraries(network_partition_test PRIVATE
            GTest::gtest
            GTest::gtest_main
        )
    else()
        target_link_libraries(network_partition_test PRIVATE
            gtest
            gtest_main
        )
    endif()

    set_target_properties(network_partition_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME NetworkPartitionTests COMMAND network_partition_test)

    gtest_discover_tests(network_partition_test
        PROPERTIES
            TIMEOUT 300
        DISCOVERY_TIMEOUT 60
    )

    # Distributed Benchmark Tests
    add_executable(distributed_benchmark_test
        distributed/distributed_benchmark_test.cpp
    )

    target_link_libraries(distributed_benchmark_test PRIVATE
        multi_node_test_framework
        database
    )

    if(GTest_FOUND)
        target_link_libraries(distributed_benchmark_test PRIVATE
            GTest::gtest
            GTest::gtest_main
        )
    else()
        target_link_libraries(distributed_benchmark_test PRIVATE
            gtest
            gtest_main
        )
    endif()

    set_target_properties(distributed_benchmark_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME DistributedBenchmarkTests COMMAND distributed_benchmark_test)

    gtest_discover_tests(distributed_benchmark_test
        PROPERTIES
            TIMEOUT 600
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Distributed multi-node tests configured (DB-011)")
else()
    if(NOT CURL_FOUND)
        message(WARNING "CURL not found - distributed tests require libcurl for Toxiproxy integration")
    endif()
endif()
