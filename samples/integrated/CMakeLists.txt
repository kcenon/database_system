cmake_minimum_required(VERSION 3.16)

##################################################
# Integrated Database System Samples
#
# Phase 6 samples demonstrating unified_database_system
##################################################

project(database_integrated_samples
    VERSION 1.0.0
    DESCRIPTION "Integrated Database System Sample Programs"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

##################################################
# Integrated Sample Programs
##################################################

# List of integrated sample programs
set(INTEGRATED_SAMPLES
    basic_usage
    async_queries
    monitoring
    migration_from_legacy
)

# Helper function to configure integrated samples
function(configure_integrated_sample SAMPLE_NAME)
    add_executable(integrated_${SAMPLE_NAME} ${SAMPLE_NAME}.cpp)

    set_target_properties(integrated_${SAMPLE_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/samples/integrated
    )

    # Include directories
    target_include_directories(integrated_${SAMPLE_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/database
    )

    # Link libraries
    target_link_libraries(integrated_${SAMPLE_NAME} PRIVATE
        integrated_database
        database
        Threads::Threads
    )

    # Link common_system if available
    if(USE_COMMON_SYSTEM)
        set(_COMMON_LIB_PATHS
            "/Users/$ENV{USER}/Sources/common_system/build"
            "/Users/$ENV{USER}/Sources/common_system/build/lib"
            "/home/$ENV{USER}/Sources/common_system/build"
            "/home/$ENV{USER}/Sources/common_system/build/lib"
        )

        foreach(_lib_path ${_COMMON_LIB_PATHS})
            if(EXISTS "${_lib_path}/libcommon_system.a" OR EXISTS "${_lib_path}")
                target_link_directories(integrated_${SAMPLE_NAME} PRIVATE ${_lib_path})
                target_link_libraries(integrated_${SAMPLE_NAME} PRIVATE common_system)
                break()
            endif()
        endforeach()
    endif()

    # Link thread_system if available
    if(USE_THREAD_SYSTEM)
        set(_THREAD_LIB_PATHS
            "/Users/$ENV{USER}/Sources/thread_system/build/lib"
            "/home/$ENV{USER}/Sources/thread_system/build/lib"
        )

        foreach(_lib_path ${_THREAD_LIB_PATHS})
            if(EXISTS "${_lib_path}")
                target_link_directories(integrated_${SAMPLE_NAME} PRIVATE ${_lib_path})
                target_link_libraries(integrated_${SAMPLE_NAME} PRIVATE ThreadSystem)
                break()
            endif()
        endforeach()
    endif()

    # Link logger_system if available
    if(USE_LOGGER_SYSTEM)
        set(_LOGGER_LIB_PATHS
            "/Users/$ENV{USER}/Sources/logger_system/build"
            "/Users/$ENV{USER}/Sources/logger_system/build/lib"
            "/home/$ENV{USER}/Sources/logger_system/build"
            "/home/$ENV{USER}/Sources/logger_system/build/lib"
        )

        foreach(_lib_path ${_LOGGER_LIB_PATHS})
            if(EXISTS "${_lib_path}/liblogger_system.a" OR EXISTS "${_lib_path}")
                target_link_directories(integrated_${SAMPLE_NAME} PRIVATE ${_lib_path})
                target_link_libraries(integrated_${SAMPLE_NAME} PRIVATE logger_system)
                break()
            endif()
        endforeach()
    endif()

    # Link monitoring_system if available
    if(USE_MONITORING_SYSTEM)
        set(_MONITORING_LIB_PATHS
            "/Users/$ENV{USER}/Sources/monitoring_system/build"
            "/Users/$ENV{USER}/Sources/monitoring_system/build/lib"
            "/home/$ENV{USER}/Sources/monitoring_system/build"
            "/home/$ENV{USER}/Sources/monitoring_system/build/lib"
        )

        foreach(_lib_path ${_MONITORING_LIB_PATHS})
            if(EXISTS "${_lib_path}/libmonitoring_system.a" OR EXISTS "${_lib_path}")
                target_link_directories(integrated_${SAMPLE_NAME} PRIVATE ${_lib_path})
                target_link_libraries(integrated_${SAMPLE_NAME} PRIVATE monitoring_system)
                break()
            endif()
        endforeach()
    endif()

    # Compiler-specific options
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(integrated_${SAMPLE_NAME} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wno-unused-parameter
        )
    elseif(MSVC)
        target_compile_options(integrated_${SAMPLE_NAME} PRIVATE
            /W4 /wd4100
        )
    endif()

    # Platform-specific definitions
    if(WIN32)
        target_compile_definitions(integrated_${SAMPLE_NAME} PRIVATE
            _WIN32_WINNT=0x0601
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    endif()

    message(STATUS "Integrated sample configured: ${SAMPLE_NAME}")
endfunction()

##################################################
# Build Each Integrated Sample
##################################################

foreach(SAMPLE ${INTEGRATED_SAMPLES})
    configure_integrated_sample(${SAMPLE})
endforeach()

##################################################
# Installation (Optional)
##################################################

# Generate list of target names for installation
set(INTEGRATED_TARGETS)
foreach(SAMPLE ${INTEGRATED_SAMPLES})
    list(APPEND INTEGRATED_TARGETS integrated_${SAMPLE})
endforeach()

# Install sample binaries
install(TARGETS ${INTEGRATED_TARGETS}
    RUNTIME DESTINATION bin/samples/integrated
    COMPONENT samples
)

# Install sample source files for reference
install(FILES
    basic_usage.cpp
    async_queries.cpp
    monitoring.cpp
    migration_from_legacy.cpp
    CMakeLists.txt
    DESTINATION share/database_system/samples/integrated
    COMPONENT samples
)

##################################################
# Summary
##################################################

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Integrated Database System Samples")
message(STATUS "========================================")
message(STATUS "Samples configured: ${INTEGRATED_SAMPLES}")
message(STATUS "Output directory: ${CMAKE_BINARY_DIR}/bin/samples/integrated")
message(STATUS "")
message(STATUS "Build with:")
message(STATUS "  cmake --build . --target integrated_basic_usage")
message(STATUS "  cmake --build . --target integrated_async_queries")
message(STATUS "  cmake --build . --target integrated_monitoring")
message(STATUS "  cmake --build . --target integrated_migration_from_legacy")
message(STATUS "========================================")
message(STATUS "")
