##################################################
# Database System: build configuration summary
#
# Prints a human-readable summary of the resolved build configuration:
# version, dependency chain, third-party deps, optional integrations,
# database features, build options, and output directories.
#
# Must be included after dependencies.cmake and install.cmake so all
# variables (BUILD_WITH_COMMON_SYSTEM, *_FOUND flags, ASIO_DETECTED_VERSION)
# are populated.
##################################################

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Database System Build Configuration:")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Dependency Chain (Tier 3):")
message(STATUS "  database_system")
message(STATUS "    ├── common_system (Tier 0): ${BUILD_WITH_COMMON_SYSTEM} [REQUIRED] - ILogger, LOG_* macros")
message(STATUS "    ├── thread_system (Tier 1): ${USE_THREAD_SYSTEM} [OPTIONAL]")
message(STATUS "    ├── monitoring_system (Tier 2): ${USE_MONITORING_SYSTEM} [OPTIONAL]")
message(STATUS "    └── container_system (Tier 1): ${USE_CONTAINER_SYSTEM} [OPTIONAL]")
message(STATUS "")

# Dependency chain verification
message(STATUS "Required Dependencies:")
if(BUILD_WITH_COMMON_SYSTEM AND common_system_FOUND)
    message(STATUS "  ✓ common_system (Tier 0) - Result<T> pattern")
else()
    message(FATAL_ERROR "  ✗ common_system - REQUIRED but not found!")
endif()

message(STATUS "")
message(STATUS "Third-Party Dependencies:")
if(ASIO_DETECTED_VERSION)
    message(STATUS "  ASIO: ${ASIO_DETECTED_VERSION} (pinned >= ${DATABASE_SYSTEM_ASIO_PINNED_VERSION})")
else()
    message(STATUS "  ASIO: not detected (pinned >= ${DATABASE_SYSTEM_ASIO_PINNED_VERSION})")
endif()


message(STATUS "")
message(STATUS "Optional Dependencies:")

if(USE_THREAD_SYSTEM AND thread_system_FOUND)
    message(STATUS "  ✓ thread_system (Tier 1)")
else()
    message(STATUS "  ○ thread_system (Tier 1) - not loaded")
endif()

if(USE_MONITORING_SYSTEM AND monitoring_system_FOUND)
    message(STATUS "  ✓ monitoring_system (Tier 3)")
else()
    message(STATUS "  ○ monitoring_system (Tier 3) - not loaded")
endif()

if(USE_CONTAINER_SYSTEM AND container_system_FOUND)
    message(STATUS "  ✓ container_system (Tier 1)")
else()
    message(STATUS "  ○ container_system (Tier 1) - not loaded")
endif()

message(STATUS "")
message(STATUS "Database Features:")
message(STATUS "  PostgreSQL support: ${USE_POSTGRESQL}")
message(STATUS "  SQLite support: ${USE_SQLITE}")
message(STATUS "  MongoDB support: ${USE_MONGODB} (experimental)")
message(STATUS "  Redis support: ${USE_REDIS} (experimental)")
message(STATUS "  Integrated database: ${BUILD_INTEGRATED_DATABASE}")
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Build samples: ${BUILD_DATABASE_SAMPLES}")
message(STATUS "  Build examples: ${BUILD_DATABASE_EXAMPLES}")
message(STATUS "  Build tests: ${USE_UNIT_TEST}")
message(STATUS "  Build integration tests: ${DATABASE_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Build benchmarks: ${DATABASE_BUILD_BENCHMARKS}")
message(STATUS "  Code coverage: ${ENABLE_COVERAGE}")
message(STATUS "")
message(STATUS "Output Directories:")
message(STATUS "  Runtime: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  Library: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "  Archive: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
message(STATUS "========================================")
