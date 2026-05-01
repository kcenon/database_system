##################################################
# Database System: samples and examples
#
# Adds the samples and examples subdirectories when the corresponding
# options are ON and the directories exist on disk.
##################################################

# Samples
if(BUILD_DATABASE_SAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples)
    add_subdirectory(samples)
    message(STATUS "Database samples will be built")
endif()

# Examples
if(BUILD_DATABASE_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
    add_subdirectory(examples)
    message(STATUS "Database examples will be built")
endif()
