##################################################
# Database System: external and ecosystem dependencies
#
# Resolves required and optional dependencies in this order:
#   1. FindSystemDependency helper module
#   2. Threads (REQUIRED)
#   3. OpenSSL (gated by USE_OPENSSL)
#   4. ASIO (header-only, version-pinned)
#   5. common_system (REQUIRED, Tier 0)
#   6. thread_system / monitoring_system / container_system (optional)
##################################################

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
