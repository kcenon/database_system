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
# Test Data Setup
##################################################

# Copy test data files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()
