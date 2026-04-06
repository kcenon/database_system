# kcenon-database-system portfile
# Pure, lightweight C++20 Core DAL library with multi-backend support

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/database_system
    REF "v${VERSION}"
    SHA512 7207570689fad1ae2afc96b4b7c79be3690a174e6f7e9c704ec470fa71f74e18ab872becbbd4a15bb3ace5a7d5eb9d5d7a5125ccca49062abab7f61b2d06185f
    HEAD_REF main
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        postgresql  USE_POSTGRESQL
        sqlite      USE_SQLITE
        mongodb     USE_MONGODB
        redis       USE_REDIS
        ecosystem   USE_THREAD_SYSTEM
        ecosystem   USE_MONITORING_SYSTEM
        ecosystem   USE_CONTAINER_SYSTEM
        ecosystem   BUILD_INTEGRATED_DATABASE
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DUSE_UNIT_TEST=OFF
        -DBUILD_DATABASE_SAMPLES=OFF
        -DDATABASE_BUILD_BENCHMARKS=OFF
        -DDATABASE_BUILD_INTEGRATION_TESTS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME database_system
    CONFIG_PATH lib/cmake/database_system
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
