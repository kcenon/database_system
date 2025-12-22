##################################################
# Database Module CMakeLists.txt
#
# Builds the database library for the database system.
# Provides database abstraction and PostgreSQL implementation.
##################################################

# Project definition
project(database
    VERSION 0.1.0.0
    DESCRIPTION "Database abstraction layer with PostgreSQL support"
    LANGUAGES CXX
)

# Include custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/database_system/cmake")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/FindSystemDependency.cmake")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/FindSystemDependency.cmake)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/cmake/FindSystemDependency.cmake")
    include(${CMAKE_SOURCE_DIR}/cmake/FindSystemDependency.cmake)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/database_system/cmake/FindSystemDependency.cmake")
    include(${CMAKE_SOURCE_DIR}/database_system/cmake/FindSystemDependency.cmake)
else()
    message(WARNING "FindSystemDependency.cmake not found - some features may not work")
endif()

set(_DATABASE_HAS_UNIFIED_HELPER FALSE)
set(_DATABASE_HELPER_CANDIDATES
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/UnifiedBuildHelper.cmake"
    "${CMAKE_SOURCE_DIR}/cmake/UnifiedBuildHelper.cmake"
)
foreach(_helper ${_DATABASE_HELPER_CANDIDATES})
    if(NOT _DATABASE_HAS_UNIFIED_HELPER AND EXISTS "${_helper}")
        include("${_helper}")
        set(_DATABASE_HAS_UNIFIED_HELPER TRUE)
        message(STATUS "Database: Loaded UnifiedBuildHelper from ${_helper}")
    endif()
endforeach()

if(_DATABASE_HAS_UNIFIED_HELPER)
    is_unified_build(_DATABASE_IS_UNIFIED_BUILD)
else()
    set(_DATABASE_IS_UNIFIED_BUILD FALSE)
endif()

##################################################
# Source Files Configuration
##################################################

# Collect all header files
set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/database_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/database_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/database_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/postgres_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/value_formatter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/sql_dialect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/condition_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/join_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql/mysql_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite/sqlite_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb/mongodb_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis/redis_manager.h
    # Phase 4 Headers
    ${CMAKE_CURRENT_SOURCE_DIR}/orm/entity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/monitoring/performance_monitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/security/secure_connection.h
    ${CMAKE_CURRENT_SOURCE_DIR}/async/async_operations.h
    # Sprint 5 Headers (Backend Plugin System)
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/backend_registry.h
    # Sprint 5.2 Headers (PostgreSQL Backend Plugin)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/postgresql_backend.h
    # Sprint 5.3 Headers (Additional Backend Plugins)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis_backend.h
    # P1-2 Headers (Immutable Query Builder)
    ${CMAKE_CURRENT_SOURCE_DIR}/query/immutable_query_builder.h
    # Phase 4.1 Headers (Proxy Mode)
    ${CMAKE_CURRENT_SOURCE_DIR}/proxy/proxy_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/proxy/proxy_connector.h
)

# Collect all source files
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/database_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/postgres_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/value_formatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/sql_dialect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/condition_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/query_builder/join_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql/mysql_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite/sqlite_manager.cpp
    # Phase 4 Sources
    ${CMAKE_CURRENT_SOURCE_DIR}/orm/entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monitoring/performance_monitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb/mongodb_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis/redis_manager.cpp
    # Sprint 5 Sources (Backend Plugin System)
    ${CMAKE_CURRENT_SOURCE_DIR}/core/database_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/backend_registry.cpp
    # Sprint 5.2 Sources (PostgreSQL Backend Plugin)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/postgresql_backend.cpp
    # Sprint 5.3 Sources (Additional Backend Plugins)
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mysql_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/sqlite_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/mongodb_backend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/redis_backend.cpp
    # P1-2 Sources (Immutable Query Builder)
    ${CMAKE_CURRENT_SOURCE_DIR}/query/immutable_query_builder.cpp
    # Phase 4.1 Sources (Proxy Mode)
    ${CMAKE_CURRENT_SOURCE_DIR}/proxy/proxy_connector.cpp
)

# Add container-based protocol serialization if container_system is available
if(USE_CONTAINER_SYSTEM AND (container_system_FOUND OR TARGET container_system OR TARGET ContainerSystem::container OR _FOUND_CONTAINER_INCLUDE))
    list(APPEND SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/database_protocol_container.cpp
    )
endif()



##################################################
# Library Target Configuration
##################################################

# Create static library
add_library(${PROJECT_NAME} STATIC
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

# Suppress warnings inherited from parent project
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wno-c++98-compat-extra-semi
        -Wno-extra-semi
        -Wno-unused-private-field
        -Wno-sign-conversion
        -Wno-conversion
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
        /wd4100  # unreferenced formal parameter
    )
endif()

if(BUILD_WITH_COMMON_SYSTEM)
    message(STATUS "Database: Enabling common_system integration")
    set(_DATABASE_COMMON_TARGET "")
    if(DEFINED common_system_TARGET AND common_system_TARGET AND TARGET ${common_system_TARGET})
        set(_DATABASE_COMMON_TARGET ${common_system_TARGET})
    elseif(TARGET common_system)
        set(_DATABASE_COMMON_TARGET common_system)
    elseif(TARGET kcenon::common)
        set(_DATABASE_COMMON_TARGET kcenon::common)
    endif()

    if(_DATABASE_COMMON_TARGET)
        target_link_libraries(${PROJECT_NAME} PUBLIC ${_DATABASE_COMMON_TARGET})
        target_compile_definitions(${PROJECT_NAME} PUBLIC BUILD_WITH_COMMON_SYSTEM)
        message(STATUS "Database: common_system integration enabled via target ${_DATABASE_COMMON_TARGET}")
    else()
        find_system_dependency_include(common_system)

        if(common_system_INCLUDE_DIR)
            message(STATUS "Found common_system at: ${common_system_INCLUDE_DIR}")
            target_include_directories(${PROJECT_NAME} PUBLIC
                $<BUILD_INTERFACE:${common_system_INCLUDE_DIR}>
            )
            target_compile_definitions(${PROJECT_NAME} PUBLIC BUILD_WITH_COMMON_SYSTEM)
            message(STATUS "Database: common_system integration enabled")
        else()
            message(WARNING "common_system not found - integration disabled. Install common_system or set BUILD_WITH_COMMON_SYSTEM=OFF to suppress this warning.")
            set(BUILD_WITH_COMMON_SYSTEM OFF CACHE BOOL "common_system not found" FORCE)
        endif()
    endif()
endif()

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add HAS_COROUTINES definition if C++20 coroutines are available
if(HAS_COROUTINES)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_COROUTINES)
endif()

##################################################
# Include Directories
##################################################

# Public include directories (for consumers of this library)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include/database_system>
)

##################################################
# Dependencies
##################################################

# Find required packages
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

# thread_system integration (optional)
if(USE_THREAD_SYSTEM)
    set(_DATABASE_THREAD_HANDLED FALSE)

    if(_DATABASE_IS_UNIFIED_BUILD)
        foreach(_candidate ThreadSystem thread_system)
            if(TARGET ${_candidate})
                target_link_libraries(${PROJECT_NAME} PUBLIC ${_candidate})
                set(_DATABASE_THREAD_HANDLED TRUE)
                message(STATUS "Database: Linked ${_candidate} target from unified build")
                break()
            endif()
        endforeach()
    endif()

    if(_DATABASE_THREAD_HANDLED)
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_THREAD_SYSTEM)
    else()
        # Standalone fallback: locate headers and optional libraries
        find_system_dependency_include(thread_system)
        set(_DATABASE_THREAD_INCLUDE_PATHS ${thread_system_INCLUDE_DIR})

        set(_FOUND_THREAD_INCLUDE FALSE)
        foreach(_path ${_DATABASE_THREAD_INCLUDE_PATHS})
            if(EXISTS "${_path}/kcenon/thread/core/thread_pool.h")
                message(STATUS "Database: Found thread_system headers at: ${_path}")
                target_include_directories(${PROJECT_NAME} PUBLIC
                    $<BUILD_INTERFACE:${_path}>
                )
                set(_FOUND_THREAD_INCLUDE TRUE)
                break()
            endif()
        endforeach()

        if(_FOUND_THREAD_INCLUDE)
            target_compile_definitions(${PROJECT_NAME} PUBLIC USE_THREAD_SYSTEM)
            message(STATUS "Database: thread_system integration enabled")

            set(_THREAD_SYSTEM_SOURCE "")
            foreach(_path ${_DATABASE_THREAD_INCLUDE_PATHS})
                if(EXISTS "${_path}/kcenon/thread/core/thread_pool.h")
                    get_filename_component(_THREAD_SYSTEM_SOURCE "${_path}" DIRECTORY)
                    break()
                endif()
            endforeach()

            if(_THREAD_SYSTEM_SOURCE AND EXISTS "${_THREAD_SYSTEM_SOURCE}/src/impl/typed_pool")
                target_include_directories(${PROJECT_NAME} PRIVATE
                    ${_THREAD_SYSTEM_SOURCE}/src
                )
            endif()
        else()
            message(WARNING "Database: thread_system headers not found - integration disabled")
            set(USE_THREAD_SYSTEM OFF CACHE BOOL "thread_system not found" FORCE)
        endif()
    endif()
endif()

# container_system integration (optional)
if(USE_CONTAINER_SYSTEM)
    set(_DATABASE_CONTAINER_HANDLED FALSE)

    if(_DATABASE_IS_UNIFIED_BUILD)
        foreach(_candidate container_system "ContainerSystem::container")
            if(TARGET ${_candidate})
                target_link_libraries(${PROJECT_NAME} PUBLIC ${_candidate})
                set(_DATABASE_CONTAINER_HANDLED TRUE)
                message(STATUS "Database: Linked ${_candidate} target from unified build")
                break()
            endif()
        endforeach()
    endif()

    if(_DATABASE_CONTAINER_HANDLED)
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_CONTAINER_SYSTEM)
    else()
        find_system_dependency(container_system)

        if(container_system_FOUND AND container_system_SOURCE_DIR)
            set(_DATABASE_CONTAINER_INCLUDE_PATHS
                "${container_system_SOURCE_DIR}"
                "${container_system_SOURCE_DIR}/include"
            )
        else()
            find_system_dependency_include(container_system)
            set(_DATABASE_CONTAINER_INCLUDE_PATHS ${container_system_INCLUDE_DIR})
        endif()

        set(_FOUND_CONTAINER_INCLUDE FALSE)
        foreach(_path ${_DATABASE_CONTAINER_INCLUDE_PATHS})
            if(EXISTS "${_path}/core/container.h" OR EXISTS "${_path}/container/core/container.h")
                message(STATUS "Database: Found container_system headers at: ${_path}")
                target_include_directories(${PROJECT_NAME} PUBLIC
                    $<BUILD_INTERFACE:${_path}>
                )
                set(_FOUND_CONTAINER_INCLUDE TRUE)
                break()
            endif()
        endforeach()

        if(_FOUND_CONTAINER_INCLUDE)
            target_compile_definitions(${PROJECT_NAME} PUBLIC USE_CONTAINER_SYSTEM)
            message(STATUS "Database: container_system integration enabled (standalone)")

            set(_DATABASE_CONTAINER_TARGET "")
            if(DEFINED container_system_TARGET AND container_system_TARGET AND TARGET ${container_system_TARGET})
                set(_DATABASE_CONTAINER_TARGET ${container_system_TARGET})
            elseif(TARGET ContainerSystem::container)
                set(_DATABASE_CONTAINER_TARGET ContainerSystem::container)
            elseif(TARGET container_system)
                set(_DATABASE_CONTAINER_TARGET container_system)
            endif()

            if(_DATABASE_CONTAINER_TARGET)
                target_link_libraries(${PROJECT_NAME} PUBLIC ${_DATABASE_CONTAINER_TARGET})
                message(STATUS "Database: Linked container_system via target: ${_DATABASE_CONTAINER_TARGET}")
            else()
                # Fallback: find and link library file directly when no target exists
                find_system_dependency_library(container_system)
                if(container_system_LIBRARY_DIR)
                    find_library(_CONTAINER_LIB
                        NAMES container_system
                        PATHS "${container_system_LIBRARY_DIR}"
                        NO_DEFAULT_PATH
                    )
                    if(_CONTAINER_LIB)
                        target_link_libraries(${PROJECT_NAME} PUBLIC ${_CONTAINER_LIB})
                        message(STATUS "Database: Linked container_system library directly: ${_CONTAINER_LIB}")
                    else()
                        message(WARNING "Database: container_system library file not found in ${container_system_LIBRARY_DIR}")
                    endif()
                else()
                    message(WARNING "Database: container_system library directory not found - linking may fail")
                endif()
            endif()
        else()
            message(WARNING "Database: container_system headers not found - integration disabled")
            set(USE_CONTAINER_SYSTEM OFF CACHE BOOL "container_system not found" FORCE)
        endif()
    endif()
endif()


# PostgreSQL dependency (optional)
if(USE_POSTGRESQL)
    find_package(libpqxx CONFIG QUIET)

    # Prefer OpenSSL 3.x for security (1.1.1 reached EOL in September 2023)
    find_package(OpenSSL 3.0 QUIET)
    if(NOT OPENSSL_FOUND)
        find_package(OpenSSL 1.1.1 QUIET)
        if(OPENSSL_FOUND)
            message(WARNING "OpenSSL 1.1.1 detected - this version reached EOL in September 2023. "
                            "Please upgrade to OpenSSL 3.x for continued security support.")
        endif()
    endif()

    if(libpqxx_FOUND AND OPENSSL_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                libpqxx::pqxx
                OpenSSL::SSL
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_POSTGRESQL)
        message(STATUS "PostgreSQL support enabled")
        message(STATUS "  libpqxx found: ${libpqxx_FOUND}")
        message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
    else()
        message(WARNING "PostgreSQL libraries not found, disabling PostgreSQL support")
        message(STATUS "  libpqxx found: ${libpqxx_FOUND}")
        message(STATUS "  OpenSSL found: ${OPENSSL_FOUND}")
        set(USE_POSTGRESQL OFF CACHE BOOL "PostgreSQL support disabled due to missing libraries" FORCE)
    endif()
endif()

# MySQL dependency (optional)
if(USE_MYSQL)
    find_package(unofficial-libmysql CONFIG QUIET)

    if(unofficial-libmysql_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                unofficial::libmysql::libmysql
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_MYSQL)
        message(STATUS "MySQL support enabled")
        message(STATUS "  libmysql found: ${unofficial-libmysql_FOUND}")
    else()
        message(WARNING "MySQL libraries not found, disabling MySQL support")
        message(STATUS "  libmysql found: ${unofficial-libmysql_FOUND}")
        set(USE_MYSQL OFF CACHE BOOL "MySQL support disabled due to missing libraries" FORCE)
    endif()
endif()

# SQLite dependency (optional)
if(USE_SQLITE)
    # Try CONFIG mode first (vcpkg)
    find_package(unofficial-sqlite3 CONFIG QUIET)

    if(unofficial-sqlite3_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                unofficial::sqlite3::sqlite3
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_SQLITE)
        message(STATUS "SQLite support enabled (CONFIG)")
        message(STATUS "  sqlite3 found: ${unofficial-sqlite3_FOUND}")
    else()
        # Try pkg-config mode (Homebrew, system package managers)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(SQLITE3 QUIET sqlite3)
            if(SQLITE3_FOUND)
                target_include_directories(${PROJECT_NAME} PUBLIC ${SQLITE3_INCLUDE_DIRS})
                target_link_libraries(${PROJECT_NAME} PUBLIC ${SQLITE3_LIBRARIES})
                target_compile_definitions(${PROJECT_NAME} PUBLIC USE_SQLITE)
                message(STATUS "SQLite support enabled (pkg-config)")
                message(STATUS "  sqlite3 version: ${SQLITE3_VERSION}")
                message(STATUS "  sqlite3 include: ${SQLITE3_INCLUDE_DIRS}")
                message(STATUS "  sqlite3 libraries: ${SQLITE3_LIBRARIES}")
            else()
                message(WARNING "SQLite libraries not found, disabling SQLite support")
                message(STATUS "  sqlite3 found: ${unofficial-sqlite3_FOUND}")
                set(USE_SQLITE OFF CACHE BOOL "SQLite support disabled due to missing libraries" FORCE)
            endif()
        else()
            message(WARNING "SQLite libraries not found, disabling SQLite support")
            message(STATUS "  sqlite3 found: ${unofficial-sqlite3_FOUND}")
            set(USE_SQLITE OFF CACHE BOOL "SQLite support disabled due to missing libraries" FORCE)
        endif()
    endif()
endif()

# MongoDB dependency (optional)
if(USE_MONGODB)
    find_package(mongocxx CONFIG QUIET)
    find_package(bsoncxx CONFIG QUIET)

    if(mongocxx_FOUND AND bsoncxx_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                mongo::mongocxx_shared
                mongo::bsoncxx_shared
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_MONGODB)
        message(STATUS "MongoDB support enabled")
        message(STATUS "  mongocxx found: ${mongocxx_FOUND}")
        message(STATUS "  bsoncxx found: ${bsoncxx_FOUND}")
    else()
        message(WARNING "MongoDB libraries not found, disabling MongoDB support")
        message(STATUS "  mongocxx found: ${mongocxx_FOUND}")
        message(STATUS "  bsoncxx found: ${bsoncxx_FOUND}")
        set(USE_MONGODB OFF CACHE BOOL "MongoDB support disabled due to missing libraries" FORCE)
    endif()
endif()

# Redis dependency (optional)
if(USE_REDIS)
    find_package(hiredis CONFIG QUIET)

    if(hiredis_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PUBLIC
                hiredis::hiredis
        )
        target_compile_definitions(${PROJECT_NAME} PUBLIC USE_REDIS)
        message(STATUS "Redis support enabled")
        message(STATUS "  hiredis found: ${hiredis_FOUND}")
    else()
        message(WARNING "Redis libraries not found, disabling Redis support")
        message(STATUS "  hiredis found: ${hiredis_FOUND}")
        set(USE_REDIS OFF CACHE BOOL "Redis support disabled due to missing libraries" FORCE)
    endif()
endif()

##################################################
# Compiler Options
##################################################

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wdeprecated-declarations
        -Wno-unused-parameter
        -Wno-unused-variable
    )
elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /WX-
        /w14996  # Enable deprecation warnings (C4996)
    )
endif()

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _WIN32_WINNT=0x0601  # Windows 7 or later
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

##################################################
# Testing Support
##################################################

# Enable testing features if building tests
if(USE_UNIT_TEST AND NOT BUILD_DATABASE_SYSTEM_AS_SUBMODULE)
    # Add any database-specific test configurations here
    set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)
endif()

##################################################
# Integrated Database System (Adapter Pattern)
##################################################

if(BUILD_INTEGRATED_DATABASE)
    # Build integrated_database as a separate module
    # Note: integrated_database depends on database, NOT the reverse
    # This avoids circular dependency issues with static linking
    add_subdirectory(integrated)

    message(STATUS "Building integrated database system with adapter pattern")
endif()

##################################################
# Summary
##################################################

message(STATUS "Database library configured:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Headers: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Dependencies: Threads, libpqxx (if enabled), libmysql (if enabled)")
