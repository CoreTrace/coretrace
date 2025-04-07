cmake_minimum_required(VERSION 3.11)

# Set policy for Boost detection
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

project(ArgumentManagerProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(WIN32)
  set(IS_WINDOWS TRUE)
elseif(APPLE)
  set(IS_MACOS TRUE)
elseif(UNIX AND NOT APPLE)
  set(IS_LINUX TRUE)
endif()

# Option to automatically install dependencies
option(AUTO_INSTALL_DEPENDENCIES "Automatically install missing dependencies" OFF)

# Make Boost optional with a warning if not found
option(REQUIRE_BOOST "Require Boost libraries (fail if not found)" OFF)

# Set Boost search paths for common locations
set(BOOST_ROOT "" CACHE PATH "Boost root directory")
set(BOOST_INCLUDEDIR "" CACHE PATH "Boost include directory")
set(BOOST_LIBRARYDIR "" CACHE PATH "Boost library directory")

# Add common Linux Boost installation paths
if(IS_LINUX)
  list(APPEND CMAKE_PREFIX_PATH
    "/usr"
    "/usr/local"
    "/usr/lib/x86_64-linux-gnu"
    "/usr/include/boost"
    "/usr/lib/boost"
  )
endif()

# For newer CMake, try to accommodate both old and new ways
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_NO_BOOST_CMAKE OFF)

# First attempt with required components
find_package(Boost COMPONENTS filesystem system program_options QUIET)

# If not found, try without components
if(NOT Boost_FOUND)
  find_package(Boost QUIET)
endif()

# Try to find Boost using pkg-config as last fallback
if(NOT Boost_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(BOOST QUIET boost boost_system boost_filesystem boost_program_options)
    if(BOOST_FOUND)
      set(Boost_INCLUDE_DIRS ${BOOST_INCLUDE_DIRS})
      set(Boost_LIBRARIES ${BOOST_LIBRARIES})
      set(Boost_FOUND TRUE)
    endif()
  endif()
endif()

# Handle the case when Boost is not found
if(NOT Boost_FOUND)
  if(IS_LINUX AND AUTO_INSTALL_DEPENDENCIES)
    # Try to automatically install Boost
    message(STATUS "Boost not found. Attempting to install dependencies...")
    
    # Make the script executable
    execute_process(
      COMMAND chmod +x ${CMAKE_SOURCE_DIR}/cmake/install_dependencies.sh
      RESULT_VARIABLE CHMOD_RESULT
    )
    
    if(CHMOD_RESULT EQUAL 0)
      # Run the dependency installation script
      execute_process(
        COMMAND ${CMAKE_SOURCE_DIR}/cmake/install_dependencies.sh
        RESULT_VARIABLE SCRIPT_RESULT
        OUTPUT_VARIABLE SCRIPT_OUTPUT
        ERROR_VARIABLE SCRIPT_ERROR
      )
      
      if(SCRIPT_RESULT EQUAL 0)
        message(STATUS "Dependencies installed successfully. Re-run CMake to continue.")
        message(FATAL_ERROR "Please run CMake again after dependencies installation.")
      else()
        message(STATUS "Script output: ${SCRIPT_OUTPUT}")
        message(STATUS "Script error: ${SCRIPT_ERROR}")
        message(FATAL_ERROR "Failed to install dependencies automatically. Please install them manually or run with elevated privileges.")
      endif()
    else()
      message(FATAL_ERROR "Failed to make dependency script executable. Please install dependencies manually.")
    endif()
  elseif(REQUIRE_BOOST)
    # Only show error if Boost is required
    if(IS_LINUX)
      message(FATAL_ERROR 
        "Boost libraries not found. Install them with:\n"
        "  sudo apt-get install libboost-all-dev\n\n"
        "Or install specific components:\n"
        "  sudo apt-get install libboost-filesystem-dev libboost-system-dev libboost-program-options-dev\n\n"
        "Then, if needed, specify the location using:\n"
        "  -DBOOST_ROOT=/path/to/boost\n"
        "  -DBOOST_INCLUDEDIR=/path/to/boost/include\n"
        "  -DBOOST_LIBRARYDIR=/path/to/boost/lib\n\n"
        "Alternatively:\n"
        "  - Run cmake with -DREQUIRE_BOOST=OFF to build without Boost support\n"
        "  - Run cmake with -DAUTO_INSTALL_DEPENDENCIES=ON to attempt automatic installation"
      )
    elseif(IS_MACOS)
      message(FATAL_ERROR 
        "Boost not found. Install it with:\n"
        "  brew install boost\n\n"
        "Alternatively, run cmake with -DREQUIRE_BOOST=OFF to build without Boost support."
      )
    endif()
  else
    # Just a warning if Boost is optional
    message(STATUS "Boost not found. Building without Boost support.")
    # Define a flag to indicate Boost is not available
    add_definitions(-DNO_BOOST_AVAILABLE)
  endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  add_definitions(-DHAS_BOOST)
endif()

set(PARSER_TYPE "CLI11" CACHE STRING "Type of parser to use (Getopt or CLI11)")
set_property(CACHE PARSER_TYPE PROPERTY STRINGS "Getopt" "CLI11")

option(USE_THREAD_SANITIZER "Activer ThreadSanitizer" OFF)
option(USE_ADDRESS_SANITIZER "Activer AddressSanitizer (Memory)" OFF)

set(COMMON_SOURCES
    src/ArgumentParser/BaseArgumentParser.cpp
    src/ArgumentParser/ArgumentManager.cpp
    src/ArgumentParser/ArgumentParserFactory.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
    main.cpp
)

set(SPECIFIC_SOURCES
    src/ArgumentParser/GetOpt/GetoptArgumentParser.cpp
    src/ArgumentParser/CLI11/CLI11ArgumentParser.cpp
)

set(SOURCES
    ${COMMON_SOURCES}
    ${SPECIFIC_SOURCES}
)

add_executable(ctrace ${SOURCES})

if (PARSER_TYPE STREQUAL "Getopt")
    target_compile_definitions(ctrace PRIVATE USE_GETOPT)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|MSVC")
    # target_compile_options(ctrace PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ctrace PRIVATE)
endif()

# Link with Boost if found
if(Boost_FOUND)
  target_link_libraries(ctrace PRIVATE ${Boost_LIBRARIES})
endif()

if (USE_THREAD_SANITIZER)
    include(${CMAKE_SOURCE_DIR}/cmake/debug/sanitizer/ThreadSanitizer.cmake)
endif()

if (USE_ADDRESS_SANITIZER)
    include(${CMAKE_SOURCE_DIR}/cmake/debug/sanitizer/AddressSanitizer.cmake)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/FetchNlohmannJson.cmake)
target_link_libraries(ctrace PRIVATE nlohmann_json::nlohmann_json)

option(USE_EXTERNAL_CLI11 "Download CLI11" OFF)
if (USE_EXTERNAL_CLI11 AND PARSER_TYPE STREQUAL "CLI11")
    include(${CMAKE_SOURCE_DIR}/cmake/CLI11.cmake)
    target_link_libraries(ctrace PRIVATE CLI11::CLI11)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/tscancode.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/flawfinder.cmake)

# Make IKOS inclusion conditional to avoid errors on Windows
option(USE_IKOS "Enable IKOS static analyzer" ON)
if(USE_IKOS AND NOT IS_WINDOWS)
    include(${CMAKE_SOURCE_DIR}/cmake/ikos.cmake)
endif()
