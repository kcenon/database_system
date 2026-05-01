cmake_minimum_required(VERSION 3.20)

##################################################
# Database System CMakeLists.txt
#
# Tier 3: Database Layer
# Pure, lightweight Core DAL library with multi-backend support.
# Provides database abstraction and PostgreSQL implementation.
#
# Dependency Chain:
#   database_system (Tier 3)
#     ├── common_system (Tier 0) [REQUIRED] - ILogger, GlobalLoggerRegistry, LOG_* macros
#     ├── thread_system (Tier 1) [OPTIONAL]
#     ├── monitoring_system (Tier 2) [OPTIONAL]
#     └── container_system (Tier 1) [OPTIONAL]
##################################################

# Project definition
project(database_system
    VERSION 1.0.0
    DESCRIPTION "Pure, Lightweight C++20 Core DAL Library"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Detect C++20 coroutine support (optional feature)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <coroutine>
    struct task {
        struct promise_type {
            task get_return_object() { return {}; }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() {}
        };
    };
    task foo() { co_return; }
    int main() {}
" HAS_COROUTINES)

if(HAS_COROUTINES)
    message(STATUS "C++20 coroutines available - async features enabled")
else()
    message(STATUS "C++20 coroutines not available - async features disabled")
endif()

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(USE_UNIT_TEST "Use unit test" ON)
option(BUILD_DATABASE_SAMPLES "Build database system samples" ON)
option(BUILD_DATABASE_EXAMPLES "Build database system usage examples" OFF)
option(USE_POSTGRESQL "Enable PostgreSQL support" ON)
option(USE_SQLITE "Enable SQLite support" OFF)
option(USE_MONGODB "Enable MongoDB backend (EXPERIMENTAL - see docs/BACKENDS.md)" OFF)
option(USE_REDIS "Enable Redis backend (EXPERIMENTAL - see docs/BACKENDS.md)" OFF)

# Experimental backend warnings
if(USE_MONGODB)
    message(WARNING
        "MongoDB support is EXPERIMENTAL and NOT recommended for production use.\n"
        "  - Test coverage is limited (no dedicated test suite)\n"
        "  - APIs may change in future releases\n"
        "  - See docs/BACKENDS.md for stabilization roadmap")
endif()

if(USE_REDIS)
    message(WARNING
        "Redis support is EXPERIMENTAL and NOT recommended for production use.\n"
        "  - Redis is not intended for persistent data storage\n"
        "  - Test coverage is limited (no dedicated test suite)\n"
        "  - APIs may change in future releases\n"
        "  - See docs/BACKENDS.md for stabilization roadmap")
endif()

option(BUILD_DATABASE "Build database module" ON)
# Security posture: OpenSSL is enabled by default so that TLS, PBKDF2-HMAC-SHA256,
# and AES-256-GCM are available for the secure_connection module out of the box.
# Opt out (USE_OPENSSL=OFF) only for minimal embedded builds that cannot ship OpenSSL;
# see docs/compliance/ISO_27001.md for the security rationale.
option(USE_OPENSSL "Enable OpenSSL-backed TLS and cryptography for secure_connection" ON)
option(USE_THREAD_SYSTEM "Enable thread_system integration for high-performance threading" ON)
option(USE_MONITORING_SYSTEM "Enable monitoring_system integration for metrics and profiling" ON)
option(USE_CONTAINER_SYSTEM "Enable container_system integration for high-performance serialization" ON)
option(BUILD_INTEGRATED_DATABASE "Build integrated database system with adapter pattern" ON)
option(DATABASE_BUILD_BENCHMARKS "Build database system benchmarks" OFF)
option(DATABASE_BUILD_INTEGRATION_TESTS "Build database system integration tests" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

# Coverage Configuration (DB-006)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage -O0 -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")

        # Find coverage tools
        find_program(GCOV_PATH gcov)
        find_program(GCOVR_PATH gcovr)

        if(NOT GCOV_PATH)
            message(WARNING "gcov not found - coverage reports may not work")
        endif()

        if(GCOVR_PATH)
            # Coverage report generation target
            add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
                COMMAND ${GCOVR_PATH}
                    --root ${CMAKE_SOURCE_DIR}
                    --exclude '.*tests/.*'
                    --exclude '.*third_party/.*'
                    --exclude '.*samples/.*'
                    --exclude '.*examples/.*'
                    --exclude '.*benchmarks/.*'
                    --html --html-details
                    --output ${CMAKE_BINARY_DIR}/coverage/index.html
                    --xml ${CMAKE_BINARY_DIR}/coverage/coverage.xml
                    --print-summary
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report..."
            )

            # Coverage threshold check target (80% line coverage)
            add_custom_target(coverage-check
                COMMAND ${GCOVR_PATH}
                    --root ${CMAKE_SOURCE_DIR}
                    --exclude '.*tests/.*'
                    --exclude '.*third_party/.*'
                    --exclude '.*samples/.*'
                    --exclude '.*examples/.*'
                    --exclude '.*benchmarks/.*'
                    --fail-under-line 80
                    --print-summary
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Checking coverage threshold (80% line coverage required)..."
            )

            message(STATUS "Coverage targets available: 'coverage', 'coverage-check'")
        else()
            message(WARNING "gcovr not found - install with 'pip install gcovr'")
        endif()
    else()
        message(WARNING "Code coverage only supported with GCC/Clang")
        set(ENABLE_COVERAGE OFF CACHE BOOL "Coverage not supported" FORCE)
    endif()
endif()

# Respect global BUILD_INTEGRATION_TESTS flag if set
if(DEFINED BUILD_INTEGRATION_TESTS)
    if(BUILD_INTEGRATION_TESTS)
        set(_DATABASE_BUILD_IT_VALUE ON)
    else()
        set(_DATABASE_BUILD_IT_VALUE OFF)
    endif()
    set(DATABASE_BUILD_INTEGRATION_TESTS ${_DATABASE_BUILD_IT_VALUE} CACHE BOOL "Build database system integration tests" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

# Platform-specific settings
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00) # Windows 10
elseif(APPLE)
    add_definitions(-DAPPLE_PLATFORM)
endif()

# Include CMake utilities
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FindSystemDependency)

# Find required packages
find_package(Threads REQUIRED)

# OpenSSL for real cryptography in secure_connection (enabled by default, see USE_OPENSSL).
# Rationale: ISO/IEC 27001 A.10 / A.13 recommend authenticated encryption for data in transit
# and keyed hash functions for credential storage. See docs/compliance/ISO_27001.md.
if(USE_OPENSSL)
    find_package(OpenSSL QUIET)
    if(OPENSSL_FOUND)
        message(STATUS "OpenSSL ${OPENSSL_VERSION} found — secure_connection will use PBKDF2/AES-256-GCM")
    else()
        message(WARNING
            "USE_OPENSSL=ON but OpenSSL was not found.\n"
            "  secure_connection will fall back to placeholder crypto (NOT for production).\n"
            "  Install OpenSSL >= 3.0 (vcpkg: 'vcpkg install openssl') or pass -DUSE_OPENSSL=OFF\n"
            "  to acknowledge the insecure build explicitly.")
    endif()
else()
    message(WARNING
        "USE_OPENSSL=OFF — secure_connection will use placeholder crypto only.\n"
        "  This is INSECURE and must not be used for production deployments.\n"
        "  See docs/compliance/ISO_27001.md for the controls this opt-out disables.")
endif()

##################################################
# ASIO dependency (required, header-only)
#
# SOUP traceability (IEC 62304): ASIO is a critical async I/O dependency.
# Version must be kept in sync with vcpkg.json "version>=" field.
#
# Version history:
#   1.29.0 - Initial pinned version
#   1.30.2 - Upgraded to ecosystem standard (aligned with network_system)
##################################################
set(DATABASE_SYSTEM_ASIO_PINNED_VERSION "1.30.2")

find_package(asio QUIET CONFIG)
if(TARGET asio::asio)
    # Detect version from vcpkg-provided headers
    get_target_property(_asio_inc asio::asio INTERFACE_INCLUDE_DIRECTORIES)
    if(_asio_inc)
        list(GET _asio_inc 0 _asio_inc_first)
        set(_asio_version_file "${_asio_inc_first}/asio/version.hpp")
        if(EXISTS "${_asio_version_file}")
            file(STRINGS "${_asio_version_file}" _asio_version_line
                REGEX "^#define ASIO_VERSION [0-9]+")
            if(_asio_version_line)
                string(REGEX MATCH "[0-9]+" _asio_version_int "${_asio_version_line}")
                math(EXPR _asio_major "${_asio_version_int} / 100000")
                math(EXPR _asio_minor "(${_asio_version_int} / 100) % 1000")
                math(EXPR _asio_patch "${_asio_version_int} % 100")
                set(ASIO_DETECTED_VERSION "${_asio_major}.${_asio_minor}.${_asio_patch}")
            endif()
        endif()
    endif()
    message(STATUS "ASIO found via CMake package (pinned: ${DATABASE_SYSTEM_ASIO_PINNED_VERSION}, detected: ${ASIO_DETECTED_VERSION})")
else()
    find_path(ASIO_INCLUDE_DIR NAMES asio.hpp DOC "Path to standalone ASIO headers")
    if(ASIO_INCLUDE_DIR)
        set(_asio_version_file "${ASIO_INCLUDE_DIR}/asio/version.hpp")
        if(EXISTS "${_asio_version_file}")
            file(STRINGS "${_asio_version_file}" _asio_version_line
                REGEX "^#define ASIO_VERSION [0-9]+")
            if(_asio_version_line)
                string(REGEX MATCH "[0-9]+" _asio_version_int "${_asio_version_line}")
                math(EXPR _asio_major "${_asio_version_int} / 100000")
                math(EXPR _asio_minor "(${_asio_version_int} / 100) % 1000")
                math(EXPR _asio_patch "${_asio_version_int} % 100")
                set(ASIO_DETECTED_VERSION "${_asio_major}.${_asio_minor}.${_asio_patch}")
            endif()
        endif()
        message(STATUS "ASIO found at: ${ASIO_INCLUDE_DIR} (pinned: ${DATABASE_SYSTEM_ASIO_PINNED_VERSION}, detected: ${ASIO_DETECTED_VERSION})")
    else()
        message(WARNING "ASIO not found - async features may not be available")
    endif()
endif()

# common_system integration (REQUIRED for Result<T> pattern)
message(STATUS "Searching for common_system...")
find_system_dependency(common_system)

# Fallback: check for TARGET directly (unified build case where PARENT_SCOPE may not propagate)
if(NOT common_system_FOUND AND TARGET common_system)
    message(STATUS "Found common_system as existing target (unified build)")
    set(common_system_FOUND TRUE)
endif()

if(common_system_FOUND)
    message(STATUS "common_system integration enabled")
    set(BUILD_WITH_COMMON_SYSTEM ON)
else()
    message(FATAL_ERROR "common_system not found! database_system requires common_system v1.0+.\n"
                        "The Result<T> pattern from common_system is now mandatory.\n"
                        "Please install common_system first (Tier 0 dependency).")
endif()

# thread_system integration (optional)
if(USE_THREAD_SYSTEM)
    message(STATUS "Searching for thread_system...")
    find_system_dependency(thread_system)

    if(thread_system_FOUND)
        message(STATUS "thread_system integration enabled")
    else()
        message(WARNING "thread_system not found - integration disabled. Install thread_system or set USE_THREAD_SYSTEM=OFF to suppress this warning.")
        set(USE_THREAD_SYSTEM OFF CACHE BOOL "thread_system not found" FORCE)
    endif()
endif()

# monitoring_system integration (optional)
if(USE_MONITORING_SYSTEM)
    message(STATUS "Searching for monitoring_system...")
    find_system_dependency(monitoring_system)

    if(monitoring_system_FOUND)
        message(STATUS "monitoring_system integration enabled")
    else()
        message(WARNING "monitoring_system not found - integration disabled. Install monitoring_system or set USE_MONITORING_SYSTEM=OFF to suppress this warning.")
        set(USE_MONITORING_SYSTEM OFF CACHE BOOL "monitoring_system not found" FORCE)
    endif()
endif()

# container_system integration (optional)
if(USE_CONTAINER_SYSTEM)
    message(STATUS "Searching for container_system...")
    find_system_dependency(container_system)

    if(container_system_FOUND)
        message(STATUS "container_system integration enabled")
    else()
        message(WARNING "container_system not found - integration disabled. Install container_system or set USE_CONTAINER_SYSTEM=OFF to suppress this warning.")
        set(USE_CONTAINER_SYSTEM OFF CACHE BOOL "container_system not found" FORCE)
    endif()
endif()


##################################################
# Modular Architecture
##################################################

# Database module
if(BUILD_DATABASE)
    add_subdirectory(database)
    message(STATUS "Database module will be built")
else()
    message(STATUS "Database module disabled")
endif()

##################################################
# Samples
##################################################

if(BUILD_DATABASE_SAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples)
    add_subdirectory(samples)
    message(STATUS "Database samples will be built")
endif()

##################################################
# Examples
##################################################

if(BUILD_DATABASE_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
    add_subdirectory(examples)
    message(STATUS "Database examples will be built")
endif()

##################################################
# Testing
##################################################

# Unit tests
if(USE_UNIT_TEST AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Database tests will be built")
endif()

##################################################
# Benchmarks
##################################################

if(DATABASE_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks)
    message(STATUS "Database benchmarks will be built")
endif()

##################################################
# Integration Tests
##################################################

if(DATABASE_BUILD_INTEGRATION_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests)
    add_subdirectory(integration_tests)
    message(STATUS "Database integration tests will be built")
endif()

##################################################
# Installation Rules
#
# Uses Tiered Install Degradation: headers and config files are always
# installed, but install(EXPORT) / export(EXPORT) are conditional on all
# PUBLIC dependencies being IMPORTED targets (i.e., already in an export
# set). This allows subdirectory builds to get headers + library + config
# while avoiding CMake generation errors from non-IMPORTED transitive deps.
##################################################

include(GNUInstallDirs)

if(BUILD_DATABASE)
    # --- Determine export capability via IMPORTED property detection ------
    set(_DB_CAN_EXPORT TRUE)
    set(_DB_NON_IMPORTED_DEPS "")

    # Check common_system (required dependency)
    foreach(_target common_system::common_system common_system)
        if(TARGET ${_target})
            get_target_property(_imp ${_target} IMPORTED)
            if(NOT _imp)
                set(_DB_CAN_EXPORT FALSE)
                list(APPEND _DB_NON_IMPORTED_DEPS "${_target}")
            endif()
            break()
        endif()
    endforeach()

    # Check thread_system (optional ecosystem dependency)
    foreach(_target thread_system::thread_system thread_system)
        if(TARGET ${_target})
            get_target_property(_imp ${_target} IMPORTED)
            if(NOT _imp)
                set(_DB_CAN_EXPORT FALSE)
                list(APPEND _DB_NON_IMPORTED_DEPS "${_target}")
            endif()
            break()
        endif()
    endforeach()

    # Check monitoring_system (optional ecosystem dependency)
    foreach(_target monitoring_system::monitoring_system monitoring_system)
        if(TARGET ${_target})
            get_target_property(_imp ${_target} IMPORTED)
            if(NOT _imp)
                set(_DB_CAN_EXPORT FALSE)
                list(APPEND _DB_NON_IMPORTED_DEPS "${_target}")
            endif()
            break()
        endif()
    endforeach()

    # Check container_system (optional ecosystem dependency)
    foreach(_target container_system::container_system container_system)
        if(TARGET ${_target})
            get_target_property(_imp ${_target} IMPORTED)
            if(NOT _imp)
                set(_DB_CAN_EXPORT FALSE)
                list(APPEND _DB_NON_IMPORTED_DEPS "${_target}")
            endif()
            break()
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _DB_NON_IMPORTED_DEPS)

    # --- Tier 1: Headers (ALWAYS) ----------------------------------------
    # Public headers live under include/kcenon/database/ and install to
    # <prefix>/include/kcenon/database/ so consumers use the canonical
    # <kcenon/database/...> form.
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )

    # --- Tier 2+3: Targets + conditional export --------------------------
    if(_DB_CAN_EXPORT)
        # Tier 2: Install library target with export set
        install(TARGETS database
            EXPORT database_system-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )

        # Tier 3: Install + build-tree export sets
        install(EXPORT database_system-targets
            FILE database_system-targets.cmake
            NAMESPACE database_system::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/database_system
        )

        export(EXPORT database_system-targets
            FILE "${CMAKE_CURRENT_BINARY_DIR}/database_system-targets.cmake"
            NAMESPACE database_system::
        )
    else()
        # Tier 2 only: Install library without EXPORT (no export set)
        install(TARGETS database
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

        message(STATUS "database_system: install(EXPORT) skipped — non-IMPORTED deps: "
            "${_DB_NON_IMPORTED_DEPS}. Headers and library are installed.")
    endif()

    # --- Tier 4: Config + version files (ALWAYS) -------------------------
    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/database_system-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/database_system-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/database_system
        PATH_VARS CMAKE_INSTALL_INCLUDEDIR
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/database_system-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/database_system-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/database_system-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/database_system
    )
endif()

##################################################
# Summary
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
