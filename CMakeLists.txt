# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.28)

project(ArgumentManagerProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_SOURCE_DIR}/cmake/CoreTraceVersion.cmake)
coretrace_resolve_version(CTRACE_VERSION_STRING CTRACE_VERSION_SOURCE)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/include/App")
configure_file(${CMAKE_SOURCE_DIR}/include/App/Version.hpp.in
               ${CMAKE_BINARY_DIR}/generated/include/App/Version.hpp
               @ONLY)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/external)
include_directories(${CMAKE_BINARY_DIR}/generated/include)

message(STATUS "CoreTrace version: ${CTRACE_VERSION_STRING} (${CTRACE_VERSION_SOURCE})")

option(USE_THREAD_SANITIZER "Activer ThreadSanitizer" OFF)
option(USE_ADDRESS_SANITIZER "Activer AddressSanitizer (Memory)" OFF)

set(SOURCES
    src/App/Config.cpp
    src/App/ToolConfig.cpp
    src/App/Files.cpp
    src/App/Runner.cpp
    src/Process/Ipc/ApiHandler.cpp
    src/Process/Ipc/GracefulShutdown.cpp
    src/Process/Ipc/HttpServer.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
    src/Process/Tools/StackAnalyzerToolImplementation.cpp
    main.cpp
)

add_executable(ctrace ${SOURCES})

# Warnings are errors on the code we own. Fetched dependencies keep their own flags.
option(CORETRACE_WERROR "Treat compiler warnings as errors" ON)

function(coretrace_enable_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if (CORETRACE_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} PRIVATE /W4)
        if (CORETRACE_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    endif()
endfunction()

if (USE_THREAD_SANITIZER)
    include(${CMAKE_SOURCE_DIR}/cmake/debug/sanitizer/ThreadSanitizer.cmake)
endif()

if (USE_ADDRESS_SANITIZER)
    include(${CMAKE_SOURCE_DIR}/cmake/debug/sanitizer/AddressSanitizer.cmake)
endif()

# === INCLUDE .CMAKE ===

include(${CMAKE_SOURCE_DIR}/cmake/FetchNlohmannJson.cmake)
target_link_libraries(ctrace PRIVATE nlohmann_json::nlohmann_json)

include(${CMAKE_SOURCE_DIR}/cmake/logger/coretraceLog.cmake)
target_link_libraries(ctrace PRIVATE coretrace::logger)

include(${CMAKE_SOURCE_DIR}/cmake/stackUsageAnalyzer.cmake)
target_link_libraries(ctrace PRIVATE coretrace::stack_usage_analyzer_lib)

include(${CMAKE_SOURCE_DIR}/cmake/httpLib.cmake)
target_link_libraries(ctrace PRIVATE httplib::httplib)

# === OPTIONAL DEPENDENCIES ===

option(USE_EXTERNAL_CLI11 "Download CLI11" OFF)

if (USE_EXTERNAL_CLI11)
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
    src/ctrace_tools/strings.cpp
)

target_link_libraries(ctrace_config_tests PRIVATE nlohmann_json::nlohmann_json coretrace::logger)

add_test(NAME ctrace_config_tests COMMAND ctrace_config_tests)

# The help text is the CLI contract: every option must be listed by `ctrace --help`.
add_test(NAME ctrace_help_lists_stack_limit COMMAND ctrace --help)
set_tests_properties(ctrace_help_lists_stack_limit PROPERTIES PASS_REGULAR_EXPRESSION "--stack-limit")
add_test(NAME ctrace_help_lists_quiet COMMAND ctrace --help)
set_tests_properties(ctrace_help_lists_quiet PROPERTIES PASS_REGULAR_EXPRESSION "--quiet")
add_test(NAME ctrace_help_lists_version COMMAND ctrace --help)
set_tests_properties(ctrace_help_lists_version PROPERTIES PASS_REGULAR_EXPRESSION "--version")

add_executable(ctrace_tool_arguments_tests
    tests/tool_arguments_tests.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
)
target_link_libraries(ctrace_tool_arguments_tests PRIVATE nlohmann_json::nlohmann_json coretrace::logger)
add_test(NAME ctrace_tool_arguments_tests COMMAND ctrace_tool_arguments_tests)

add_executable(ctrace_files_tests tests/files_tests.cpp src/App/Files.cpp src/ctrace_tools/strings.cpp)
target_link_libraries(ctrace_files_tests PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME ctrace_files_tests COMMAND ctrace_files_tests)

add_executable(ctrace_tool_output_tests tests/tool_output_tests.cpp)
add_test(NAME ctrace_tool_output_tests COMMAND ctrace_tool_output_tests)

add_executable(ctrace_process_tests tests/process_tests.cpp)
target_link_libraries(ctrace_process_tests PRIVATE coretrace::logger)
add_test(NAME ctrace_process_tests COMMAND ctrace_process_tests)

# Server mode: concurrent run_analysis requests through the API handler (ROADMAP C3).
add_executable(ctrace_server_concurrency_tests
    tests/server_concurrency_tests.cpp
    src/App/Runner.cpp
    src/Process/Ipc/ApiHandler.cpp
    src/Process/Ipc/GracefulShutdown.cpp
    src/Process/Ipc/HttpServer.cpp
    src/App/Files.cpp
    src/App/ToolConfig.cpp
    src/Process/Tools/StackAnalyzerToolImplementation.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
)

target_link_libraries(ctrace_server_concurrency_tests
    PRIVATE nlohmann_json::nlohmann_json coretrace::logger coretrace::stack_usage_analyzer_lib
            httplib::httplib)

add_test(NAME ctrace_server_concurrency_tests
    COMMAND ctrace_server_concurrency_tests ${CMAKE_SOURCE_DIR})

add_executable(ctrace_server_http_tests
    tests/server_http_tests.cpp
    src/App/Runner.cpp
    src/Process/Ipc/ApiHandler.cpp
    src/Process/Ipc/GracefulShutdown.cpp
    src/Process/Ipc/HttpServer.cpp
    src/App/Files.cpp
    src/App/ToolConfig.cpp
    src/Process/Tools/StackAnalyzerToolImplementation.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
)
target_link_libraries(ctrace_server_http_tests
    PRIVATE nlohmann_json::nlohmann_json coretrace::logger coretrace::stack_usage_analyzer_lib
            httplib::httplib)
add_test(NAME ctrace_server_http_tests COMMAND ctrace_server_http_tests)

# K4: the bridge report explains every option forwarded to the analyzer, at verbose level.
add_test(NAME ctrace_bridge_report_is_logged
    COMMAND ctrace --verbose --config ${CMAKE_SOURCE_DIR}/config/tool-config.json
        --input ${CMAKE_SOURCE_DIR}/tests/double_free.c
        --report-file ${CMAKE_BINARY_DIR}/ctrace-bridge-report.json)
set_tests_properties(ctrace_bridge_report_is_logged PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[applied\\] --mode - value='ir'")

# K7: the version is a release tag, a git describe string, or the dev fallback.
add_test(NAME ctrace_version_is_resolved COMMAND ctrace --version)
set_tests_properties(ctrace_version_is_resolved PROPERTIES
    PASS_REGULAR_EXPRESSION "^ctrace (v[0-9]+\\.[0-9]+|dev)")

# An unreachable IPC socket must be reported, not crash the process.
add_test(NAME ctrace_socket_failure_is_reported
    COMMAND ctrace --ipc socket --ipc-path /nonexistent/ctrace-test.sock
        --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke cppcheck)
set_tests_properties(ctrace_socket_failure_is_reported PROPERTIES
    PASS_REGULAR_EXPRESSION "deprecated"
    FAIL_REGULAR_EXPRESSION "terminating due to uncaught exception")

# Compatibility gate with the pinned coretrace-stack-analyzer library (see ROADMAP.md K10).
add_executable(ctrace_stack_analyzer_bridge_tests
    tests/stack_analyzer_bridge_tests.cpp
    src/Process/Tools/StackAnalyzerToolImplementation.cpp
    src/App/ToolConfig.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
)

target_link_libraries(ctrace_stack_analyzer_bridge_tests
    PRIVATE nlohmann_json::nlohmann_json coretrace::logger coretrace::stack_usage_analyzer_lib)

add_test(NAME ctrace_stack_analyzer_bridge_tests
    COMMAND ctrace_stack_analyzer_bridge_tests ${CMAKE_SOURCE_DIR})

# End-to-end: the CLI drives the analyzer with the canonical config and reports a summary.
add_test(NAME ctrace_stack_analyzer_cli_smoke
    COMMAND ctrace
        --config ${CMAKE_SOURCE_DIR}/config/tool-config.json
        --input ${CMAKE_SOURCE_DIR}/tests/double_free.c
        --report-file ${CMAKE_BINARY_DIR}/ctrace-smoke-report.json)
set_tests_properties(ctrace_stack_analyzer_cli_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION
        "\\(ctrace_stack_analyzer\\) Diagnostics summary: info=0, warning=0, error=1")

# Applied last so every executable we own exists by now.
foreach(coretrace_target
    ctrace
    ctrace_config_tests
    ctrace_tool_arguments_tests
    ctrace_files_tests
    ctrace_tool_output_tests
    ctrace_process_tests
    ctrace_server_concurrency_tests
    ctrace_server_http_tests
    ctrace_stack_analyzer_bridge_tests
)
    coretrace_enable_warnings(${coretrace_target})
endforeach()
