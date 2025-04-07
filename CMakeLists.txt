cmake_minimum_required(VERSION 3.11)

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

# Boost dependency
if(IS_LINUX OR IS_MACOS)
  find_package(Boost REQUIRED COMPONENTS filesystem system program_options)
  if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found. Install it with 'brew install boost' on macOS or 'apt-get install libboost-all-dev' on Linux.")
  endif()
elseif(IS_WINDOWS)
  # On Windows, make Boost optional or handle differently
  find_package(Boost COMPONENTS filesystem system program_options)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
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
