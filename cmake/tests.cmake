##################################################
# Database System: test, benchmark, and integration test wiring
#
# Adds the unit tests, benchmark suite, and integration test subdirectory
# when the relevant options are ON and the directory exists. Test discovery
# inside `tests/` is owned by tests/CMakeLists.txt; this module only wires
# the top-level enable_testing() and the conditional add_subdirectory()s.
##################################################

# Unit tests
if(USE_UNIT_TEST AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Database tests will be built")
endif()

# Benchmarks
if(DATABASE_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks)
    message(STATUS "Database benchmarks will be built")
endif()

# Integration tests
if(DATABASE_BUILD_INTEGRATION_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests)
    add_subdirectory(integration_tests)
    message(STATUS "Database integration tests will be built")
endif()
