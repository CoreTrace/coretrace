# Find or fetch Boost

# Options for Boost dependency
option(USE_SYSTEM_BOOST "Use system-installed Boost" ON)
option(BOOST_FROM_SCRIPT "Install Boost using script if not found" ON)
set(BOOST_REQUIRED_VERSION "1.71.0" CACHE STRING "Minimum required Boost version")
set(BOOST_REQUIRED_COMPONENTS system filesystem program_options CACHE STRING "Required Boost components")

# Try to find Boost on the system
if(USE_SYSTEM_BOOST)
    find_package(Boost ${BOOST_REQUIRED_VERSION} COMPONENTS ${BOOST_REQUIRED_COMPONENTS})
endif()

# If not found and script installation is enabled, try to install using script
if(NOT Boost_FOUND AND BOOST_FROM_SCRIPT)
    message(STATUS "Boost not found. Attempting to install via script...")
    
    # Path to the script relative to the project source directory
    set(BOOST_INSTALL_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/install_boost.sh")
    
    # Make sure the script is executable
    execute_process(
        COMMAND chmod +x ${BOOST_INSTALL_SCRIPT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    
    # Execute the installation script
    execute_process(
        COMMAND ${BOOST_INSTALL_SCRIPT} --prefix=${CMAKE_BINARY_DIR}/boost_install
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE SCRIPT_RESULT
    )
    
    if(NOT SCRIPT_RESULT EQUAL 0)
        message(WARNING "Failed to install Boost via script (exit code: ${SCRIPT_RESULT})")
    else()
        # Try to find Boost again, now that it's been installed
        set(BOOST_ROOT ${CMAKE_BINARY_DIR}/boost_install)
        find_package(Boost ${BOOST_REQUIRED_VERSION} COMPONENTS ${BOOST_REQUIRED_COMPONENTS})
    endif()
endif()

# Check if Boost was found
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find Boost. Please install Boost manually or enable BOOST_FROM_SCRIPT.")
endif()

message(STATUS "Using Boost ${Boost_VERSION}")
