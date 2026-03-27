cmake_minimum_required(VERSION 3.28)

project(ArgumentManagerProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external)

set(PARSER_TYPE "CLI11" CACHE STRING "Type of parser to use (Getopt or CLI11)")
set_property(CACHE PARSER_TYPE PROPERTY STRINGS "Getopt" "CLI11")

option(USE_THREAD_SANITIZER "Activer ThreadSanitizer" OFF)
option(USE_ADDRESS_SANITIZER "Activer AddressSanitizer (Memory)" OFF)

set(COMMON_SOURCES
    src/ArgumentParser/BaseArgumentParser.cpp
    src/ArgumentParser/ArgumentManager.cpp
    src/ArgumentParser/ArgumentParserFactory.cpp
    src/App/Config.cpp
    src/App/ToolConfig.cpp
    src/App/Files.cpp
    src/App/Runner.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
    src/Process/Tools/StackAnalyzerToolImplementation.cpp
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

if (USE_THREAD_SANITIZER)
    include(${CMAKE_SOURCE_DIR}/cmake/debug/sanitizer/ThreadSanitizer.cmake)
endif()

if (USE_ADDRESS_SANITIZER)
    include(${CMAKE_SOURCE_DIR}/cmake/debug/sanitizer/AddressSanitizer.cmake)
endif()

# === INCLUDE .CMAKE ===

include(${CMAKE_SOURCE_DIR}/cmake/FetchNlohmannJson.cmake)
target_link_libraries(ctrace PRIVATE nlohmann_json::nlohmann_json)

include(${CMAKE_SOURCE_DIR}/cmake/stackUsageAnalyzer.cmake)
target_link_libraries(ctrace PRIVATE coretrace::stack_usage_analyzer_lib)

include(${CMAKE_SOURCE_DIR}/cmake/logger/coretraceLog.cmake)
target_link_libraries(ctrace PRIVATE coretrace::logger)

include(${CMAKE_SOURCE_DIR}/cmake/httpLib.cmake)
target_link_libraries(ctrace PRIVATE httplib::httplib)

# === OPTIONAL DEPENDENCIES ===

option(USE_EXTERNAL_CLI11 "Download CLI11" OFF)

if (USE_EXTERNAL_CLI11 AND PARSER_TYPE STREQUAL "CLI11")
    include(${CMAKE_SOURCE_DIR}/cmake/CLI11.cmake)
    target_link_libraries(ctrace PRIVATE CLI11::CLI11)
endif()

option(ENABLE_TSCANCODE "Download Tscancode" OFF)

if (ENABLE_TSCANCODE)
    include(${CMAKE_SOURCE_DIR}/cmake/tscancode.cmake)
endif()

option(ENABLE_FLAWFINDER "Enable flawfinder external project integration" OFF)

if (ENABLE_FLAWFINDER)
    include(${CMAKE_SOURCE_DIR}/cmake/flawfinder.cmake)
endif()
# include(${CMAKE_SOURCE_DIR}/cmake/ikos.cmake)

# ============
#  FORMATTING
# ============
add_custom_target(format
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/format.sh"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Run clang-format on source files")

add_custom_target(format-check
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/format-check.sh"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Verify clang-format compliance")

# ============
#   INSTALL
# ============
include(GNUInstallDirs)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(ctrace PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
    )

    install(TARGETS ctrace
        RUNTIME_DEPENDENCY_SET ctrace_deps
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(RUNTIME_DEPENDENCY_SET ctrace_deps
        PRE_EXCLUDE_REGEXES
            "^libc\\.so"
            "^libm\\.so"
            "^libdl\\.so"
            "^librt\\.so"
            "^libpthread\\.so"
            "^ld-linux"
            "^libstdc\\+\\+"
            "^libgcc_s"
        DIRECTORIES
            "/usr/lib/llvm-20/lib"
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    install(TARGETS ctrace
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/config/
    DESTINATION config
    FILES_MATCHING
        PATTERN "*.json"
        PATTERN "*.txt"
)

# ============
#    TESTS
# ============
enable_testing()

add_executable(ctrace_config_tests
    tests/config_parser_tests.cpp
    src/App/ToolConfig.cpp
    src/App/Config.cpp
    src/ArgumentParser/BaseArgumentParser.cpp
    src/ArgumentParser/ArgumentManager.cpp
    src/ArgumentParser/ArgumentParserFactory.cpp
    src/ArgumentParser/CLI11/CLI11ArgumentParser.cpp
    src/ArgumentParser/GetOpt/GetoptArgumentParser.cpp
    src/ctrace_tools/strings.cpp
)

target_link_libraries(ctrace_config_tests PRIVATE nlohmann_json::nlohmann_json coretrace::logger)

add_test(NAME ctrace_config_tests COMMAND ctrace_config_tests)
