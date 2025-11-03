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

    message(STATUS "Integrated database configuration test configured")
endif()

##################################################
# Test Data Setup
##################################################

# Copy test data files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()