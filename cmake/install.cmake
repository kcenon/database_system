##################################################
# Database System: installation rules
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

    # Legacy forwarding shims (issue #582). Mirror every public header at
    # <prefix>/include/database/<rest>.h so external consumers using the
    # pre-#577 <database/...> form keep building, with a deprecation
    # #pragma message redirecting them to <kcenon/database/...>.
    if(NOT DATABASE_DISABLE_LEGACY_HEADERS)
        install(DIRECTORY legacy_include/database
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h"
        )
    endif()

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
