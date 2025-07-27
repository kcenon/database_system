
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was DatabaseSystemConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

##################################################
# DatabaseSystem Package Configuration
##################################################

include(CMakeFindDependencyMacro)

# Find required dependencies
find_dependency(Threads REQUIRED)
find_dependency(PostgreSQL REQUIRED)

# Find container system dependency
find_dependency(ContainerSystem QUIET)
if(NOT ContainerSystem_FOUND)
    # Container system is required
    message(FATAL_ERROR "DatabaseSystem requires ContainerSystem")
endif()

# Include targets
include("${CMAKE_CURRENT_LIST_DIR}/DatabaseSystemTargets.cmake")

# Verify targets exist
check_required_components(DatabaseSystem)

# Set variables for compatibility
set(DatabaseSystem_FOUND TRUE)
set(DATABASESYSTEM_FOUND TRUE)

# Provide information about the package
set(DatabaseSystem_VERSION 1.0.0)
set(DatabaseSystem_INCLUDE_DIRS "/database_system")
set(DatabaseSystem_LIBRARIES DatabaseSystem::database)

# Legacy variables for backward compatibility
set(DATABASESYSTEM_VERSION ${DatabaseSystem_VERSION})
set(DATABASESYSTEM_INCLUDE_DIRS ${DatabaseSystem_INCLUDE_DIRS})
set(DATABASESYSTEM_LIBRARIES ${DatabaseSystem_LIBRARIES})

message(STATUS "Found DatabaseSystem: ${DatabaseSystem_VERSION}")
