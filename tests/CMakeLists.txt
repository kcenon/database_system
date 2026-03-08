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
# SQLite Backend Tests (DB-001)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(sqlite_backend_test
        sqlite_backend_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(sqlite_backend_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(sqlite_backend_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(sqlite_backend_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME SQLiteBackendTests COMMAND sqlite_backend_test)

    gtest_discover_tests(sqlite_backend_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "SQLite backend tests configured (DB-001)")
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

    # Connection string builder test
    add_executable(integrated_connection_string_builder_test
        integrated/test_connection_string_builder.cpp
    )

    target_link_libraries(integrated_connection_string_builder_test PRIVATE
        integrated_database
        Threads::Threads
    )

    set_target_properties(integrated_connection_string_builder_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedConnectionStringBuilderTest COMMAND integrated_connection_string_builder_test)

    message(STATUS "Integrated database configuration test configured")
    message(STATUS "Integrated database logger adapter test configured")
    message(STATUS "Integrated database monitoring adapter test configured")
    message(STATUS "Integrated database thread adapter test configured")
    message(STATUS "Integrated database coordinator test configured")
    message(STATUS "Integrated database unified system test configured (Phase 6)")
    message(STATUS "Integrated database connection string builder test configured")
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

    # SQL Dialect Tests (Issue #332)
    add_executable(sql_dialect_test
        query_builder/sql_dialect_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(sql_dialect_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(sql_dialect_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(sql_dialect_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME SQLDialectTests COMMAND sql_dialect_test)

    gtest_discover_tests(sql_dialect_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "SQL dialect tests configured (Issue #332)")
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
# DI Service Registration Tests (Issue #369)
##################################################

if(BUILD_WITH_COMMON_SYSTEM AND (GTest_FOUND OR gtest_FOUND))
    add_executable(di_service_registration_test
        di/test_service_registration.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(di_service_registration_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(di_service_registration_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    target_include_directories(di_service_registration_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    set_target_properties(di_service_registration_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME DIServiceRegistrationTests COMMAND di_service_registration_test)

    gtest_discover_tests(di_service_registration_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "DI service registration tests configured (Issue #369)")
endif()

##################################################
# ORM Entity Metadata Tests (Issue #377)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(entity_metadata_test
        orm/test_entity_metadata.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(entity_metadata_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(entity_metadata_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(entity_metadata_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME EntityMetadataTests COMMAND entity_metadata_test)

    gtest_discover_tests(entity_metadata_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "ORM entity metadata tests configured (Issue #377)")
endif()

##################################################
# Immutable Query Builder Tests (Issue #377)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(immutable_query_builder_test
        query/test_immutable_query_builder.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(immutable_query_builder_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(immutable_query_builder_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(immutable_query_builder_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME ImmutableQueryBuilderTests COMMAND immutable_query_builder_test)

    gtest_discover_tests(immutable_query_builder_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Immutable query builder tests configured (Issue #377)")
endif()

##################################################
# Protocol Serializer Tests (Issue #378)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(protocol_serializer_test
        protocol/test_protocol_serializer.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(protocol_serializer_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(protocol_serializer_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(protocol_serializer_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME ProtocolSerializerTests COMMAND protocol_serializer_test)

    gtest_discover_tests(protocol_serializer_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Protocol serializer tests configured (Issue #378)")
endif()

##################################################
# Performance Monitor Tests (Issue #379)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(performance_monitor_test
        monitoring/test_performance_monitor.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(performance_monitor_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(performance_monitor_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(performance_monitor_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME PerformanceMonitorTests COMMAND performance_monitor_test)

    gtest_discover_tests(performance_monitor_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Performance monitor tests configured (Issue #379)")
endif()

##################################################
# Async Unit Tests (Issue #369)
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(async_operations_test
        async/test_async_operations.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(async_operations_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(async_operations_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(async_operations_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME AsyncOperationsTests COMMAND async_operations_test)

    gtest_discover_tests(async_operations_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Async operations unit tests configured (Issue #369)")
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
# Backend Registry Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(backend_registry_test
        backend_registry_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(backend_registry_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(backend_registry_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(backend_registry_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME BackendRegistryTests COMMAND backend_registry_test)

    gtest_discover_tests(backend_registry_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Backend registry tests configured")
endif()

##################################################
# Backend Base CRTP Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(backend_base_test
        backend_base_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(backend_base_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(backend_base_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(backend_base_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME BackendBaseTests COMMAND backend_base_test)

    gtest_discover_tests(backend_base_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Backend base CRTP tests configured")
endif()

##################################################
# Backend Interface Contract Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(backend_contract_test
        backend_contract_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(backend_contract_test PRIVATE
            database
            database_mocks
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(backend_contract_test PRIVATE
            database
            database_mocks
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(backend_contract_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME BackendContractTests COMMAND backend_contract_test)

    gtest_discover_tests(backend_contract_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Backend interface contract tests configured")
endif()

##################################################
# PostgreSQL Backend Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(postgresql_backend_test
        postgresql_backend_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(postgresql_backend_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(postgresql_backend_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(postgresql_backend_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME PostgreSQLBackendTests COMMAND postgresql_backend_test)

    gtest_discover_tests(postgresql_backend_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "PostgreSQL backend tests configured")
endif()

##################################################
# MongoDB Backend Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(mongodb_backend_test
        mongodb_backend_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(mongodb_backend_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(mongodb_backend_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(mongodb_backend_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME MongoDBBackendTests COMMAND mongodb_backend_test)

    gtest_discover_tests(mongodb_backend_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "MongoDB backend tests configured")
endif()

##################################################
# Redis Backend Tests
##################################################

if(GTest_FOUND OR gtest_FOUND)
    add_executable(redis_backend_test
        redis_backend_test.cpp
    )

    if(GTest_FOUND)
        target_link_libraries(redis_backend_test PRIVATE
            database
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )
    else()
        target_link_libraries(redis_backend_test PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
        )
    endif()

    set_target_properties(redis_backend_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME RedisBackendTests COMMAND redis_backend_test)

    gtest_discover_tests(redis_backend_test
        PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Redis backend tests configured")
endif()

##################################################
# Test Data Setup
##################################################

# Copy test data files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()

