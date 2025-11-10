# FindSystemDependency.cmake
# Reusable CMake module for finding system dependencies
#
# Usage:
#   include(FindSystemDependency)
#   find_system_dependency(thread_system)
#
# This will search for the dependency in:
# 1. Environment variable ${NAME}_ROOT
# 2. Standard source locations (macOS, Linux, Windows)
# 3. Relative paths (sibling directories)
# 4. CMake CONFIG mode (installed packages, vcpkg)
#
# Sets:
#   ${NAME}_FOUND - TRUE if dependency was found
#   ${NAME}_DIR - Build directory path (for CONFIG mode)

function(find_system_dependency NAME)
    # Check if already found
    if(${NAME}_FOUND)
        return()
    endif()

    # Build list of search paths
    set(_SEARCH_PATHS)

    # 1. Environment variable (highest priority)
    if(DEFINED ENV{${NAME}_ROOT})
        list(APPEND _SEARCH_PATHS "$ENV{${NAME}_ROOT}")
    endif()

    # 2. Platform-specific standard locations
    if(APPLE)
        list(APPEND _SEARCH_PATHS "/Users/$ENV{USER}/Sources/${NAME}")
    elseif(UNIX)
        list(APPEND _SEARCH_PATHS "/home/$ENV{USER}/Sources/${NAME}")
    elseif(WIN32)
        list(APPEND _SEARCH_PATHS "C:/Users/$ENV{USERNAME}/Sources/${NAME}")
    endif()

    # 3. Relative paths (sibling directories)
    list(APPEND _SEARCH_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/../${NAME}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../${NAME}"
    )

    # Try to find source directory
    foreach(_path ${_SEARCH_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt")
            message(STATUS "Found ${NAME} source at: ${_path}")
            set(${NAME}_DIR "${_path}/build" CACHE PATH "${NAME} build directory" FORCE)
            set(${NAME}_FOUND TRUE PARENT_SCOPE)
            set(${NAME}_SOURCE_DIR "${_path}" CACHE PATH "${NAME} source directory" FORCE)
            return()
        endif()
    endforeach()

    # Try CONFIG mode if not found in source paths
    find_package(${NAME} CONFIG QUIET)
    if(${NAME}_FOUND)
        message(STATUS "Found ${NAME} via CONFIG mode")
        set(${NAME}_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    # Not found
    set(${NAME}_FOUND FALSE PARENT_SCOPE)
endfunction()

# Helper function to find system dependency include paths
function(find_system_dependency_include NAME)
    # Build list of include search paths
    set(_INCLUDE_PATHS)

    # 1. Environment variable
    if(DEFINED ENV{${NAME}_ROOT})
        list(APPEND _INCLUDE_PATHS "$ENV{${NAME}_ROOT}/include")
    endif()

    # 2. Platform-specific standard locations
    if(APPLE)
        list(APPEND _INCLUDE_PATHS "/Users/$ENV{USER}/Sources/${NAME}/include")
    elseif(UNIX)
        list(APPEND _INCLUDE_PATHS "/home/$ENV{USER}/Sources/${NAME}/include")
    elseif(WIN32)
        list(APPEND _INCLUDE_PATHS "C:/Users/$ENV{USERNAME}/Sources/${NAME}/include")
    endif()

    # 3. Relative paths
    list(APPEND _INCLUDE_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/../${NAME}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../${NAME}/include"
    )

    # Try to find include directory
    foreach(_path ${_INCLUDE_PATHS})
        if(EXISTS "${_path}")
            set(${NAME}_INCLUDE_DIR "${_path}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Not found
    set(${NAME}_INCLUDE_DIR "" PARENT_SCOPE)
endfunction()

# Helper function to find system dependency libraries
function(find_system_dependency_library NAME)
    # Build list of library search paths
    set(_LIB_PATHS)

    # 1. Environment variable
    if(DEFINED ENV{${NAME}_ROOT})
        list(APPEND _LIB_PATHS "$ENV{${NAME}_ROOT}/build/lib")
        list(APPEND _LIB_PATHS "$ENV{${NAME}_ROOT}/build")
    endif()

    # 2. Platform-specific standard locations
    if(APPLE)
        list(APPEND _LIB_PATHS "/Users/$ENV{USER}/Sources/${NAME}/build/lib")
        list(APPEND _LIB_PATHS "/Users/$ENV{USER}/Sources/${NAME}/build")
    elseif(UNIX)
        list(APPEND _LIB_PATHS "/home/$ENV{USER}/Sources/${NAME}/build/lib")
        list(APPEND _LIB_PATHS "/home/$ENV{USER}/Sources/${NAME}/build")
    elseif(WIN32)
        list(APPEND _LIB_PATHS "C:/Users/$ENV{USERNAME}/Sources/${NAME}/build/lib")
        list(APPEND _LIB_PATHS "C:/Users/$ENV{USERNAME}/Sources/${NAME}/build")
    endif()

    # Try to find library directory
    foreach(_path ${_LIB_PATHS})
        if(EXISTS "${_path}")
            set(${NAME}_LIBRARY_DIR "${_path}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Not found
    set(${NAME}_LIBRARY_DIR "" PARENT_SCOPE)
endfunction()
