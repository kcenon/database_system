# kcenon-database-system portfile
# Pure, lightweight C++20 Core DAL library with multi-backend support

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/database_system
    REF 26ad04ca3ebe28a64e23aa95d4f0104aac019c3e
    SHA512 0  # TODO: Update with actual SHA512 hash after release
    HEAD_REF main
)

# Feature-based backend selection
set(DB_USE_POSTGRESQL OFF)
if("postgresql" IN_LIST FEATURES)
    set(DB_USE_POSTGRESQL ON)
endif()

set(DB_USE_SQLITE OFF)
if("sqlite" IN_LIST FEATURES)
    set(DB_USE_SQLITE ON)
endif()

set(DB_USE_MONGODB OFF)
if("mongodb" IN_LIST FEATURES)
    set(DB_USE_MONGODB ON)
endif()

set(DB_USE_REDIS OFF)
if("redis" IN_LIST FEATURES)
    set(DB_USE_REDIS ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DUSE_POSTGRESQL=${DB_USE_POSTGRESQL}
        -DUSE_SQLITE=${DB_USE_SQLITE}
        -DUSE_MONGODB=${DB_USE_MONGODB}
        -DUSE_REDIS=${DB_USE_REDIS}
        -DUSE_UNIT_TEST=OFF
        -DBUILD_DATABASE_SAMPLES=OFF
        -DDATABASE_BUILD_BENCHMARKS=OFF
        -DDATABASE_BUILD_INTEGRATION_TESTS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DUSE_THREAD_SYSTEM=OFF
        -DUSE_MONITORING_SYSTEM=OFF
        -DUSE_CONTAINER_SYSTEM=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME DatabaseSystem
    CONFIG_PATH lib/cmake/DatabaseSystem
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
