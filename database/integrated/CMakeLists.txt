##################################################
# Integrated Database System CMakeLists.txt
#
# Builds the integrated database system with adapter pattern
# for monitoring_system, thread_system integration.
# Logging uses common_system's ILogger and LOG_* macros.
##################################################

cmake_minimum_required(VERSION 3.16)

# Library name
set(INTEGRATED_DB_TARGET integrated_database)

##################################################
# Source Files
##################################################

set(INTEGRATED_DB_HEADERS
    # Core
    core/configuration.h
    core/database_coordinator.h
    # Adapters
    adapters/logger_adapter.h
    adapters/monitoring_adapter.h
    adapters/thread_adapter.h
    # Logger Backends (common_system based)
    adapters/backends/logger_backend.h
    adapters/backends/common_logger_backend.h
    adapters/backends/fallback_logger_backend.h
    adapters/backends/null_logger_backend.h
    # Monitoring Backends
    adapters/backends/monitoring_backend.h
    adapters/backends/fallback_monitoring_backend.h
    adapters/backends/null_monitoring_backend.h
    # Thread Backends
    adapters/backends/thread_backend.h
    adapters/backends/fallback_thread_backend.h
    adapters/backends/null_thread_backend.h
    # Unified System
    unified_database_system.h
)

set(INTEGRATED_DB_SOURCES
    # Core
    core/database_coordinator.cpp
    # Adapters
    adapters/logger_adapter.cpp
    adapters/monitoring_adapter.cpp
    adapters/thread_adapter.cpp
    # Logger Backends (common_system based - always include)
    adapters/backends/common_logger_backend.cpp
    adapters/backends/fallback_logger_backend.cpp
    # Monitoring Backends (always include fallback)
    adapters/backends/fallback_monitoring_backend.cpp
    # Thread Backends (always include fallback)
    adapters/backends/fallback_thread_backend.cpp
    # Unified System
    unified_database_system.cpp
)

# Conditionally add system_monitoring_backend if monitoring_system is available
if(USE_MONITORING_SYSTEM AND monitoring_system_FOUND)
    list(APPEND INTEGRATED_DB_SOURCES
        adapters/backends/system_monitoring_backend.cpp
    )
    # Define compile flag so monitoring_adapter can use it
    set(HAVE_SYSTEM_MONITORING_BACKEND ON)
endif()

##################################################
# Create Library Target
##################################################

if(INTEGRATED_DB_SOURCES)
    # If we have source files, create a regular library
    add_library(${INTEGRATED_DB_TARGET} ${INTEGRATED_DB_SOURCES})

    target_include_directories(${INTEGRATED_DB_TARGET}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>  # For #include "integrated/..."
            $<INSTALL_INTERFACE:include>
    )

    # Add common_system include path if available
    if(BUILD_WITH_COMMON_SYSTEM AND common_system_FOUND)
        find_system_dependency_include(common_system)
        if(common_system_INCLUDE_DIR)
            target_include_directories(${INTEGRATED_DB_TARGET}
                PUBLIC
                    $<BUILD_INTERFACE:${common_system_INCLUDE_DIR}>
            )
        endif()
    endif()

    # Add monitoring_system include path if available
    if(USE_MONITORING_SYSTEM)
        find_system_dependency_include(monitoring_system)
        if(monitoring_system_INCLUDE_DIR)
            target_include_directories(${INTEGRATED_DB_TARGET}
                PUBLIC
                    $<BUILD_INTERFACE:${monitoring_system_INCLUDE_DIR}>
            )
        endif()
    endif()
else()
    # Header-only for now (Phase 1)
    add_library(${INTEGRATED_DB_TARGET} INTERFACE)

    target_include_directories(${INTEGRATED_DB_TARGET}
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            $<INSTALL_INTERFACE:include>
    )
endif()

##################################################
# Compiler Definitions
##################################################

# Common system integration (Mandatory)
if(BUILD_WITH_COMMON_SYSTEM AND common_system_FOUND)
    if(TARGET ${INTEGRATED_DB_TARGET})
        if(INTEGRATED_DB_SOURCES)
            target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC USE_COMMON_SYSTEM)
        else()
            target_compile_definitions(${INTEGRATED_DB_TARGET} INTERFACE USE_COMMON_SYSTEM)
        endif()
    endif()
    message(STATUS "Integrated DB: common_system integration enabled")
endif()

# Thread system integration
# NOTE: No longer need USE_THREAD_SYSTEM compile definition with backend pattern
# Backend selection happens at runtime
if(USE_THREAD_SYSTEM)
    message(STATUS "Integrated DB: thread_system backend available (runtime selection)")
endif()

# common_system logging integration
# common_logger_backend is always built when common_system is available
if(BUILD_WITH_COMMON_SYSTEM AND common_system_FOUND)
    if(INTEGRATED_DB_SOURCES)
        target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC HAVE_COMMON_LOGGER_BACKEND)
    else()
        target_compile_definitions(${INTEGRATED_DB_TARGET} INTERFACE HAVE_COMMON_LOGGER_BACKEND)
    endif()
    message(STATUS "Integrated DB: common_system logger backend enabled (runtime selection)")
endif()

# Monitoring system integration
# Add compile definition when system_monitoring_backend is built
if(USE_MONITORING_SYSTEM AND monitoring_system_FOUND)
    if(INTEGRATED_DB_SOURCES)
        target_compile_definitions(${INTEGRATED_DB_TARGET} PUBLIC HAVE_SYSTEM_MONITORING_BACKEND)
    else()
        target_compile_definitions(${INTEGRATED_DB_TARGET} INTERFACE HAVE_SYSTEM_MONITORING_BACKEND)
    endif()
    message(STATUS "Integrated DB: monitoring_system backend available (runtime selection)")
endif()

##################################################
# Dependencies
##################################################

# Link external systems (if found)

# common_system (Mandatory)
if(BUILD_WITH_COMMON_SYSTEM AND common_system_FOUND AND TARGET common_system)
    if(INTEGRATED_DB_SOURCES)
        target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC common_system)
    else()
        target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE common_system)
    endif()
    message(STATUS "Integrated DB: Linked with common_system")
endif()

# thread_system
if(USE_THREAD_SYSTEM AND TARGET thread_system)
    if(INTEGRATED_DB_SOURCES)
        target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC thread_system)
    else()
        target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE thread_system)
    endif()
    message(STATUS "Integrated DB: Linked with thread_system")
endif()

# monitoring_system
if(USE_MONITORING_SYSTEM)
    # Try to find monitoring_system library
    if(TARGET monitoring_system)
        if(INTEGRATED_DB_SOURCES)
            target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC monitoring_system)
        else()
            target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE monitoring_system)
        endif()
        message(STATUS "Integrated DB: Linked with monitoring_system target")
    else()
        # Try to link directly with monitoring_system library
        find_system_dependency_library(monitoring_system)
        set(_MONITORING_LIB_PATHS ${monitoring_system_LIBRARY_DIR})

        foreach(_path ${_MONITORING_LIB_PATHS})
            if(EXISTS "${_path}/libmonitoring_system.a")
                if(INTEGRATED_DB_SOURCES)
                    target_link_libraries(${INTEGRATED_DB_TARGET} PUBLIC
                        "${_path}/libmonitoring_system.a"
                    )
                else()
                    target_link_libraries(${INTEGRATED_DB_TARGET} INTERFACE
                        "${_path}/libmonitoring_system.a"
                    )
                endif()
                message(STATUS "Integrated DB: Linked with monitoring_system library at ${_path}")
                break()
            endif()
        endforeach()
    endif()
endif()

##################################################
# C++ Standard
##################################################

if(INTEGRATED_DB_SOURCES)
    target_compile_features(${INTEGRATED_DB_TARGET} PUBLIC cxx_std_20)
else()
    target_compile_features(${INTEGRATED_DB_TARGET} INTERFACE cxx_std_20)
endif()

##################################################
# Installation
##################################################

install(TARGETS ${INTEGRATED_DB_TARGET}
    EXPORT database_system-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY core/ adapters/
    DESTINATION include/database_system/database/integrated
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

message(STATUS "Integrated database system configured")
