##################################################
# Integrated Database System Tests
#
# Tests for adapter pattern integration:
# - Phase 1: Configuration
# - Phase 2: Logger Adapter
# - Phase 3: Monitoring Adapter
# - Phase 4: Thread Adapter
##################################################

cmake_minimum_required(VERSION 3.16)

##################################################
# Test 1: Configuration Test
##################################################

add_executable(integrated_configuration_test
    test_configuration.cpp
)

target_link_libraries(integrated_configuration_test PRIVATE
    integrated_database
)

set_target_properties(integrated_configuration_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME IntegratedConfigurationTest
         COMMAND integrated_configuration_test)

##################################################
# Test 2: Logger Adapter Test
##################################################

add_executable(integrated_logger_adapter_test
    test_logger_adapter.cpp
)

target_link_libraries(integrated_logger_adapter_test PRIVATE
    integrated_database
)

set_target_properties(integrated_logger_adapter_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME IntegratedLoggerAdapterTest
         COMMAND integrated_logger_adapter_test)

##################################################
# Test 3: Monitoring Adapter Test
##################################################

add_executable(integrated_monitoring_adapter_test
    test_monitoring_adapter.cpp
)

target_link_libraries(integrated_monitoring_adapter_test PRIVATE
    integrated_database
)

set_target_properties(integrated_monitoring_adapter_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME IntegratedMonitoringAdapterTest
         COMMAND integrated_monitoring_adapter_test)

##################################################
# Test 4: Thread Adapter Test
##################################################

add_executable(integrated_thread_adapter_test
    test_thread_adapter.cpp
)

target_link_libraries(integrated_thread_adapter_test PRIVATE
    integrated_database
)

set_target_properties(integrated_thread_adapter_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME IntegratedThreadAdapterTest
         COMMAND integrated_thread_adapter_test)

##################################################
# Test 5: Database Coordinator Test
##################################################

add_executable(integrated_coordinator_test
    test_database_coordinator.cpp
)

target_link_libraries(integrated_coordinator_test PRIVATE
    integrated_database
)

set_target_properties(integrated_coordinator_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME IntegratedCoordinatorTest
         COMMAND integrated_coordinator_test)

##################################################
# Test 6: Unified Database System Test (Phase 6)
##################################################

add_executable(integrated_unified_system_test
    test_unified_database_system.cpp
)

target_link_libraries(integrated_unified_system_test PRIVATE
    integrated_database
)

set_target_properties(integrated_unified_system_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME IntegratedUnifiedSystemTest
         COMMAND integrated_unified_system_test)

##################################################
# Test 7: Container Protocol Test (Optional)
##################################################

if(USE_CONTAINER_SYSTEM)
    add_executable(integrated_container_protocol_test
        test_container_protocol.cpp
    )

    target_link_libraries(integrated_container_protocol_test PRIVATE
        database
        gtest
        gtest_main
    )

    target_compile_definitions(integrated_container_protocol_test PRIVATE
        USE_CONTAINER_SYSTEM
    )

    set_target_properties(integrated_container_protocol_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME IntegratedContainerProtocolTest
             COMMAND integrated_container_protocol_test)

    message(STATUS "Container protocol test configured")
endif()

##################################################
# All Integrated Tests Target
##################################################

set(_INTEGRATED_TEST_TARGETS
    integrated_configuration_test
    integrated_logger_adapter_test
    integrated_monitoring_adapter_test
    integrated_thread_adapter_test
    integrated_coordinator_test
    integrated_unified_system_test
)

if(USE_CONTAINER_SYSTEM)
    list(APPEND _INTEGRATED_TEST_TARGETS integrated_container_protocol_test)
endif()

add_custom_target(integrated_tests
    DEPENDS ${_INTEGRATED_TEST_TARGETS}
)

message(STATUS "Integrated tests configured (Phases 1-6${USE_CONTAINER_SYSTEM})")
