##################################################
# Database System: build options
#
# Centralizes every option(...) declaration for the project, grouped by
# concern (general/build, backends, integrations, tests, install).
##################################################

# General build options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_DATABASE "Build database module" ON)
option(BUILD_DATABASE_SAMPLES "Build database system samples" ON)
option(BUILD_DATABASE_EXAMPLES "Build database system usage examples" OFF)
option(BUILD_INTEGRATED_DATABASE "Build integrated database system with adapter pattern" ON)

# Database backend options
option(USE_POSTGRESQL "Enable PostgreSQL support" ON)
option(USE_SQLITE "Enable SQLite support" OFF)
option(USE_MONGODB "Enable MongoDB backend (EXPERIMENTAL - see docs/BACKENDS.md)" OFF)
option(USE_REDIS "Enable Redis backend (EXPERIMENTAL - see docs/BACKENDS.md)" OFF)

# Security posture: OpenSSL is enabled by default so that TLS, PBKDF2-HMAC-SHA256,
# and AES-256-GCM are available for the secure_connection module out of the box.
# Opt out (USE_OPENSSL=OFF) only for minimal embedded builds that cannot ship OpenSSL;
# see docs/compliance/ISO_27001.md for the security rationale.
option(USE_OPENSSL "Enable OpenSSL-backed TLS and cryptography for secure_connection" ON)

# Ecosystem integration options
option(USE_THREAD_SYSTEM "Enable thread_system integration for high-performance threading" ON)
option(USE_MONITORING_SYSTEM "Enable monitoring_system integration for metrics and profiling" ON)
option(USE_CONTAINER_SYSTEM "Enable container_system integration for high-performance serialization" ON)

# Testing and coverage options
option(USE_UNIT_TEST "Use unit test" ON)
option(DATABASE_BUILD_BENCHMARKS "Build database system benchmarks" OFF)
option(DATABASE_BUILD_INTEGRATION_TESTS "Build database system integration tests" ON)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

# Install options
# Legacy <database/...> include path support (issue #582).
# When ON (default), forwarding shims under legacy_include/database/ are exposed
# in the build interface and installed at <prefix>/include/database/. They emit
# a #pragma message redirecting consumers to the canonical <kcenon/database/...>
# path. Set OFF for consumers that have already migrated; the shims are
# scheduled for removal in version 2.0.0.
option(DATABASE_DISABLE_LEGACY_HEADERS "Skip installing forwarding shims at legacy <database/...> paths" OFF)

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

# Respect global BUILD_INTEGRATION_TESTS flag if set
if(DEFINED BUILD_INTEGRATION_TESTS)
    if(BUILD_INTEGRATION_TESTS)
        set(_DATABASE_BUILD_IT_VALUE ON)
    else()
        set(_DATABASE_BUILD_IT_VALUE OFF)
    endif()
    set(DATABASE_BUILD_INTEGRATION_TESTS ${_DATABASE_BUILD_IT_VALUE} CACHE BOOL "Build database system integration tests" FORCE)
endif()
