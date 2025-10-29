##################################################
# Database System Integration Tests CMakeLists.txt
#
# Comprehensive integration testing suite for database_system
##################################################

cmake_minimum_required(VERSION 3.16)

# Find or fetch Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found - fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    # Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    message(STATUS "GTest fetched successfully")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${GTEST_INCLUDE_DIRS}
)

# Collect all test source files
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/scenarios/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/performance/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/failures/*.cpp"
)

# Create integration test executable
add_executable(database_integration_tests
    ${INTEGRATION_TEST_SOURCES}
)

# Link libraries
if(TARGET GTest::GTest)
    # Using system-installed GTest
    target_link_libraries(database_integration_tests
        PRIVATE
            database
            GTest::GTest
            GTest::Main
            Threads::Threads
    )
else()
    # Using FetchContent GTest
    target_link_libraries(database_integration_tests
        PRIVATE
            database
            gtest
            gtest_main
            Threads::Threads
    )
endif()

# Set C++ standard
set_target_properties(database_integration_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Enable SQLite support for integration tests
target_compile_definitions(database_integration_tests
    PRIVATE
        USE_SQLITE=1
)

# Conditionally enable common_system integration
if(BUILD_WITH_COMMON_SYSTEM)
    target_compile_definitions(database_integration_tests
        PRIVATE
            BUILD_WITH_COMMON_SYSTEM=1
    )
endif()

# Add test discovery
include(GoogleTest)
gtest_discover_tests(database_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES
        LABELS "integration"
)

# Add custom target to run only integration tests
add_custom_target(database_run_integration_tests
    COMMAND database_integration_tests --gtest_color=yes
    DEPENDS database_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running database system integration tests"
)

# Coverage target (optional, for CI/CD)
if(CMAKE_BUILD_TYPE MATCHES Debug AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(database_integration_tests
        PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
    )
    target_link_options(database_integration_tests
        PRIVATE
            --coverage
    )

    add_custom_target(integration_coverage
        COMMAND lcov --capture --directory . --output-file coverage.info --ignore-errors inconsistent,mismatch,gcov
        COMMAND lcov --remove coverage.info '/usr/*' '*/tests/*' '*/gtest/*' --output-file coverage_filtered.info --ignore-errors inconsistent,unused
        COMMAND lcov --list coverage_filtered.info
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating integration test coverage report"
        DEPENDS database_integration_tests
    )
endif()

# Installation (optional)
install(TARGETS database_integration_tests
    RUNTIME DESTINATION bin/tests
    COMPONENT tests
)

message(STATUS "Database integration tests configured:")
message(STATUS "  Test sources: ${INTEGRATION_TEST_SOURCES}")
message(STATUS "  Test executable: database_integration_tests")
