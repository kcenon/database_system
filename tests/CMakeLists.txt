##################################################
# Database System Tests Configuration
##################################################

# Enable testing
enable_testing()

##################################################
# Common Compiler Options for Tests
##################################################

# Function to apply deprecation warnings to test targets
function(apply_test_compiler_options target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wdeprecated-declarations
            -Wno-unused-parameter
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /WX-
            /w14996  # Enable deprecation warnings (C4996)
        )
    endif()
endfunction()

# Find test dependencies
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    find_package(gtest QUIET)
endif()
find_package(benchmark QUIET)

##################################################
# Mock Framework Library (DB-014)
##################################################

add_library(database_mocks STATIC
    mocks/mock_database.cpp
    mocks/mock_expectations.cpp
    mocks/mock_backend.cpp
    mocks/mock_backend_expectations.cpp
)

target_include_directories(database_mocks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(database_mocks PUBLIC
    database
    Threads::Threads
)

set_target_properties(database_mocks PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

message(STATUS "Mock framework library configured (DB-014)")

##################################################
# Unit Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    # Create unit test executable
    add_executable(database_unit_tests
        unit_tests.cpp
        proxy_connector_test.cpp
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
    
    # Apply deprecation warnings
    apply_test_compiler_options(database_unit_tests)

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

