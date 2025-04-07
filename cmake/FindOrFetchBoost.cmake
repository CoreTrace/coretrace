# Find or fetch Boost

# Handle CMP0167 policy (FindBoost module is deprecated)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Options for Boost dependency
option(USE_SYSTEM_BOOST "Use system-installed Boost" ON)
option(BOOST_FROM_SCRIPT "Install Boost using script if not found" ON)
set(BOOST_REQUIRED_VERSION "1.71.0" CACHE STRING "Minimum required Boost version")
set(BOOST_REQUIRED_COMPONENTS system filesystem program_options CACHE STRING "Required Boost components")

# Try to find Boost on the system
if(USE_SYSTEM_BOOST)
    message(STATUS "Searching for system Boost...")
    find_package(Boost ${BOOST_REQUIRED_VERSION} COMPONENTS ${BOOST_REQUIRED_COMPONENTS} QUIET)
    
    if(Boost_FOUND)
        message(STATUS "Found system Boost ${Boost_VERSION}")
    else()
        message(STATUS "System Boost not found or does not meet version requirements")
    endif()
endif()

# If not found and script installation is enabled, try to install using script
if(NOT Boost_FOUND AND BOOST_FROM_SCRIPT)
    message(STATUS "Attempting to install Boost via script...")
    
    # Path to the script relative to the project source directory
    set(BOOST_INSTALL_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/install_boost.sh")
    
    # Make sure the script is executable
    execute_process(
        COMMAND chmod +x ${BOOST_INSTALL_SCRIPT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    
    # Create install directory
    set(BOOST_INSTALL_DIR "${CMAKE_BINARY_DIR}/boost_install")
    file(MAKE_DIRECTORY ${BOOST_INSTALL_DIR})
    
    # Execute the installation script
    message(STATUS "Running Boost installation script...")
    execute_process(
        COMMAND ${BOOST_INSTALL_SCRIPT} --prefix=${BOOST_INSTALL_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE SCRIPT_RESULT
        OUTPUT_VARIABLE SCRIPT_OUTPUT
        ERROR_VARIABLE SCRIPT_ERROR
    )
    
    if(NOT SCRIPT_RESULT EQUAL 0)
        message(WARNING "Failed to install Boost via script (exit code: ${SCRIPT_RESULT})")
        message(STATUS "Script output: ${SCRIPT_OUTPUT}")
        message(STATUS "Script error: ${SCRIPT_ERROR}")
    else()
        message(STATUS "Boost installation script completed successfully")
        
        # Try to find Boost again with the installed version
        set(BOOST_ROOT ${BOOST_INSTALL_DIR})
        set(Boost_NO_SYSTEM_PATHS ON)
        
        find_package(Boost ${BOOST_REQUIRED_VERSION} COMPONENTS ${BOOST_REQUIRED_COMPONENTS})
        
        if(Boost_FOUND)
            message(STATUS "Successfully using installed Boost ${Boost_VERSION} from ${BOOST_INSTALL_DIR}")
        else()
            message(WARNING "Could not find Boost after installation attempt")
        endif()
    endif()
endif()

# Final check if Boost was found
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find or install Boost. Please install Boost manually or check build logs for errors.")
else()
    # Set up Boost targets if they don't already exist (for compatibility)
    if(NOT TARGET Boost::boost)
        add_library(Boost::boost INTERFACE IMPORTED)
        set_target_properties(Boost::boost PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
        )
    endif()
    
    foreach(COMPONENT ${BOOST_REQUIRED_COMPONENTS})
        string(TOUPPER ${COMPONENT} UPPER_COMPONENT)
        if(NOT TARGET Boost::${COMPONENT})
            add_library(Boost::${COMPONENT} INTERFACE IMPORTED)
            set_target_properties(Boost::${COMPONENT} PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${Boost_${UPPER_COMPONENT}_LIBRARY}"
            )
        endif()
    endforeach()
    
    message(STATUS "Boost configuration complete - version ${Boost_VERSION}")
endif()
