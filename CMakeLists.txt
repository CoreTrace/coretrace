# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.28)

project(ArgumentManagerProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# No C++20 modules here. Built with Clang and Ninja, CMake would otherwise scan every source
# for them with clang-scan-deps, which Debian and Ubuntu ship apart from clang (clang-tools).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

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
    src/App/ShippedDefaults.cpp
    src/App/ToolConfig.cpp
    src/App/Files.cpp
    src/App/Runner.cpp
    src/Process/Ipc/ApiHandler.cpp
    src/Process/Ipc/GracefulShutdown.cpp
    src/Process/Ipc/HttpServer.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
    src/Process/Tools/CppCheckToolImplementation.cpp
    src/Process/Tools/FlawfinderToolImplementation.cpp
    src/Process/Tools/PythonAnalyzerToolImplementation.cpp
    src/Process/Tools/Sarif.cpp
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

# Source-based coverage of the code we own (scripts/ci/coverage.sh reads it). Our targets are
# built at -O0 so that every line maps to what ran; fetched dependencies are not instrumented.
option(CORETRACE_COVERAGE "Instrument CoreTrace's own targets for llvm-cov (Clang only)" OFF)
if (CORETRACE_COVERAGE AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "CORETRACE_COVERAGE needs Clang: it uses source-based coverage")
endif()

function(coretrace_enable_coverage target)
    if (CORETRACE_COVERAGE)
        target_compile_options(${target} PRIVATE -O0 -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${target} PRIVATE -fprofile-instr-generate)
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
include(${CMAKE_SOURCE_DIR}/cmake/clangHeaders.cmake)
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

option(ENABLE_PYTHON_ANALYZER
    "Build coretrace-python-analyzer as a standalone executable (Nuitka) shipped with ctrace" OFF)
if(ENABLE_PYTHON_ANALYZER)
    include(${CMAKE_SOURCE_DIR}/cmake/pythonAnalyzer.cmake)
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

    # glibc is the system's: the archive runs on any glibc at least as old as the build
    # machine's. libstdc++ and libgcc_s are shipped, as the compiler may be newer than the
    # system's (GCC 13 on Ubuntu 22.04 for the release) and they are backward compatible.
    install(RUNTIME_DEPENDENCY_SET ctrace_deps
        PRE_EXCLUDE_REGEXES
            "^libc\\.so"
            "^libm\\.so"
            "^libdl\\.so"
            "^librt\\.so"
            "^libpthread\\.so"
            "^ld-linux"
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
    src/App/ShippedDefaults.cpp
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
    src/Process/Tools/CppCheckToolImplementation.cpp
    src/Process/Tools/FlawfinderToolImplementation.cpp
    src/Process/Tools/PythonAnalyzerToolImplementation.cpp
    src/Process/Tools/Sarif.cpp
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

add_executable(ctrace_report_file_tests tests/report_file_tests.cpp)
add_test(NAME ctrace_report_file_tests COMMAND ctrace_report_file_tests)

# The unified diagnostic model and the per-tool parsers that feed it.
add_executable(ctrace_tool_diagnostics_tests
    tests/tool_diagnostics_tests.cpp
    src/Process/Tools/CppCheckToolImplementation.cpp
    src/Process/Tools/FlawfinderToolImplementation.cpp
    src/Process/Tools/PythonAnalyzerToolImplementation.cpp
    src/Process/Tools/Sarif.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
)
target_link_libraries(ctrace_tool_diagnostics_tests PRIVATE nlohmann_json::nlohmann_json coretrace::logger)
add_test(NAME ctrace_tool_diagnostics_tests
    COMMAND ctrace_tool_diagnostics_tests ${CMAKE_SOURCE_DIR})

# Each file reaches only the tools of its language.
add_executable(ctrace_tool_routing_tests
    tests/tool_routing_tests.cpp
    src/Process/Tools/CppCheckToolImplementation.cpp
    src/Process/Tools/FlawfinderToolImplementation.cpp
    src/Process/Tools/PythonAnalyzerToolImplementation.cpp
    src/Process/Tools/Sarif.cpp
    src/Process/Tools/StackAnalyzerToolImplementation.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
    src/App/ToolConfig.cpp
    src/ctrace_tools/mangle.cpp
    src/ctrace_tools/languageType.cpp
    src/ctrace_tools/strings.cpp
)
target_link_libraries(ctrace_tool_routing_tests
    PRIVATE nlohmann_json::nlohmann_json coretrace::logger coretrace::stack_usage_analyzer_lib)
add_test(NAME ctrace_tool_routing_tests COMMAND ctrace_tool_routing_tests)

add_executable(ctrace_exit_policy_tests tests/exit_policy_tests.cpp)
add_test(NAME ctrace_exit_policy_tests COMMAND ctrace_exit_policy_tests)

# The exit code is the CI gate: 0 clean, 1 usage, 2 findings at/above --fail-on, 3 incomplete.
add_test(NAME ctrace_exit_2_on_error_finding
    COMMAND sh -c "$<TARGET_FILE:ctrace> --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke cppcheck; test $? -eq 2")
add_test(NAME ctrace_exit_0_with_fail_on_none
    COMMAND sh -c "$<TARGET_FILE:ctrace> --fail-on none --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke cppcheck; test $? -eq 0")
add_test(NAME ctrace_exit_0_when_below_threshold
    COMMAND sh -c "$<TARGET_FILE:ctrace> --input ${CMAKE_SOURCE_DIR}/tests/dead_code.cc --invoke cppcheck; test $? -eq 0")
add_test(NAME ctrace_exit_3_when_a_tool_cannot_run
    COMMAND sh -c "$<TARGET_FILE:ctrace> --config ${CMAKE_SOURCE_DIR}/tests/missing-tool-config.json --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke cppcheck; test $? -eq 3")
add_test(NAME ctrace_help_lists_fail_on COMMAND ctrace --help)
set_tests_properties(ctrace_help_lists_fail_on PROPERTIES PASS_REGULAR_EXPRESSION "--fail-on")
# Nothing to analyze is a usage error. A SARIF report that cannot be written is a warning: the
# log still reaches stdout and the verdict stands.
add_test(NAME ctrace_exit_1_without_input
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --invoke cppcheck 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 1 && printf '%s\\n' \"$out\" | grep -q 'No input files resolved'")
add_test(NAME ctrace_unwritable_sarif_report_keeps_the_verdict
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --sarif-format --fail-on none --invoke cppcheck --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --report-file ${CMAKE_SOURCE_DIR}/tests/double_free.c/report.sarif 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 0 && printf '%s\\n' \"$out\" | grep -q 'Unable to write the SARIF report' && printf '%s\\n' \"$out\" | grep -q '\"name\": \"cppcheck\"'")

# External tool findings feed the same counters as the stack analyzer; a tool whose output
# was not interpreted says so instead of reporting zero.
add_test(NAME ctrace_cppcheck_diagnostics_are_counted
    COMMAND ctrace --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke cppcheck)
set_tests_properties(ctrace_cppcheck_diagnostics_are_counted PROPERTIES
    PASS_REGULAR_EXPRESSION "\\(cppcheck\\) Diagnostics summary: info=0, warning=0, error=1")
# cppcheck runs with its warning-class checks enabled: style findings are counted as warnings.
add_test(NAME ctrace_cppcheck_style_checks_are_enabled
    COMMAND ctrace --fail-on none --input ${CMAKE_SOURCE_DIR}/tests/dead_code.cc --invoke cppcheck)
set_tests_properties(ctrace_cppcheck_style_checks_are_enabled PROPERTIES
    PASS_REGULAR_EXPRESSION "\\(cppcheck\\) Diagnostics summary: info=0, warning=3, error=0")
add_test(NAME ctrace_uninterpreted_tool_is_reported
    COMMAND ctrace --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke ikos)
set_tests_properties(ctrace_uninterpreted_tool_is_reported PROPERTIES
    PASS_REGULAR_EXPRESSION "\\(ikos\\) Diagnostics summary: not available")

add_executable(ctrace_process_tests tests/process_tests.cpp)
target_link_libraries(ctrace_process_tests PRIVATE coretrace::logger)
add_test(NAME ctrace_process_tests COMMAND ctrace_process_tests)

# Server mode: concurrent run_analysis requests through the API handler (ROADMAP C3).
add_executable(ctrace_server_concurrency_tests
    tests/server_concurrency_tests.cpp
    src/App/Runner.cpp
    src/Process/Ipc/ApiHandler.cpp
    src/App/ShippedDefaults.cpp
    src/Process/Ipc/GracefulShutdown.cpp
    src/Process/Ipc/HttpServer.cpp
    src/App/Files.cpp
    src/App/ToolConfig.cpp
    src/Process/Tools/CppCheckToolImplementation.cpp
    src/Process/Tools/FlawfinderToolImplementation.cpp
    src/Process/Tools/PythonAnalyzerToolImplementation.cpp
    src/Process/Tools/Sarif.cpp
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
    COMMAND ctrace_server_concurrency_tests ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

add_executable(ctrace_server_http_tests
    tests/server_http_tests.cpp
    src/App/Runner.cpp
    src/Process/Ipc/ApiHandler.cpp
    src/App/ShippedDefaults.cpp
    src/Process/Ipc/GracefulShutdown.cpp
    src/Process/Ipc/HttpServer.cpp
    src/App/Files.cpp
    src/App/ToolConfig.cpp
    src/Process/Tools/CppCheckToolImplementation.cpp
    src/Process/Tools/FlawfinderToolImplementation.cpp
    src/Process/Tools/PythonAnalyzerToolImplementation.cpp
    src/Process/Tools/Sarif.cpp
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

# The server as `ctrace --ipc serve` runs it: an analysis over HTTP, then a clean shutdown.
add_executable(ctrace_server_cli_tests tests/server_cli_tests.cpp)
target_link_libraries(ctrace_server_cli_tests
    PRIVATE nlohmann_json::nlohmann_json httplib::httplib)
add_test(NAME ctrace_server_cli_tests
    COMMAND ctrace_server_cli_tests $<TARGET_FILE:ctrace> ${CMAKE_SOURCE_DIR})
set_tests_properties(ctrace_server_cli_tests PROPERTIES TIMEOUT 120)
# The bind policy holds from the command line: no token, no host beyond the loopback. A
# server that started anyway would never return, hence the timeout.
add_test(NAME ctrace_serve_refuses_an_exposed_host_without_token
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --ipc serve --serve-host 0.0.0.0 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 1 && printf '%s\\n' \"$out\" | grep -q 'Refusing to serve'")
set_tests_properties(ctrace_serve_refuses_an_exposed_host_without_token PROPERTIES TIMEOUT 30)

# K4: the bridge report explains every option forwarded to the analyzer, at verbose level.
add_test(NAME ctrace_bridge_report_is_logged
    COMMAND ctrace --verbose --config ${CMAKE_SOURCE_DIR}/config/tool-config.json
        --input ${CMAKE_SOURCE_DIR}/tests/double_free.c
        --report-file ${CMAKE_BINARY_DIR}/ctrace-bridge-report.json)
set_tests_properties(ctrace_bridge_report_is_logged PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[applied\\] --mode - value='ir'")

# K7: the version is a release tag, a git describe string (which is a bare commit hash in a
# shallow clone without tags, as on CI), or the dev fallback.
add_test(NAME ctrace_version_is_resolved COMMAND ctrace --version)
set_tests_properties(ctrace_version_is_resolved PROPERTIES
    PASS_REGULAR_EXPRESSION "^ctrace (v[0-9]+\\.[0-9]+|dev|[0-9a-f]+)")

# An unreachable IPC socket must be reported, not crash the process.
add_test(NAME ctrace_socket_failure_is_reported
    COMMAND ctrace --ipc socket --ipc-path /nonexistent/ctrace-test.sock
        --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke cppcheck)
set_tests_properties(ctrace_socket_failure_is_reported PROPERTIES
    PASS_REGULAR_EXPRESSION "deprecated"
    FAIL_REGULAR_EXPRESSION "terminating due to uncaught exception")

# A reachable socket receives the reports; a reader that goes away is an error, not a SIGPIPE.
find_package(Threads REQUIRED)
add_executable(ctrace_ipc_socket_tests tests/ipc_socket_tests.cpp)
target_link_libraries(ctrace_ipc_socket_tests PRIVATE coretrace::logger Threads::Threads)
add_test(NAME ctrace_ipc_socket_tests COMMAND ctrace_ipc_socket_tests)
add_test(NAME ctrace_ipc_socket_through_the_cli
    COMMAND ctrace_ipc_socket_tests --ctrace $<TARGET_FILE:ctrace>
        ${CMAKE_SOURCE_DIR}/tests/double_free.c)

# Compatibility gate with the pinned coretrace-stack-analyzer library (see ROADMAP.md K10).
add_executable(ctrace_stack_analyzer_bridge_tests
    tests/stack_analyzer_bridge_tests.cpp
    src/Process/Tools/CppCheckToolImplementation.cpp
    src/Process/Tools/FlawfinderToolImplementation.cpp
    src/Process/Tools/PythonAnalyzerToolImplementation.cpp
    src/Process/Tools/Sarif.cpp
    src/Process/Tools/StackAnalyzerToolImplementation.cpp
    src/Process/Tools/TscancodeToolImplementation.cpp
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

# The default stack analyzer models are found next to the executable: no --config needed.
add_test(NAME ctrace_default_models_without_config
    COMMAND ctrace --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke ctrace_stack_analyzer
        --report-file ${CMAKE_BINARY_DIR}/ctrace-default-models-report.json)
set_tests_properties(ctrace_default_models_without_config PROPERTIES
    PASS_REGULAR_EXPRESSION
        "\\(ctrace_stack_analyzer\\) Diagnostics summary: info=0, warning=0, error=1")

# --sarif-format: one SARIF log for the whole run, one run per tool, on stdout and in the
# report file (which no longer receives the analyzer's own JSON in that mode).
set(ctrace_merged_sarif ${CMAKE_BINARY_DIR}/ctrace-merged.sarif)
add_test(NAME ctrace_sarif_is_merged_per_tool
    COMMAND ctrace --sarif-format --fail-on none --invoke cppcheck,ctrace_stack_analyzer
        --config ${CMAKE_SOURCE_DIR}/config/tool-config.json
        --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --report-file ${ctrace_merged_sarif})
set_tests_properties(ctrace_sarif_is_merged_per_tool PROPERTIES
    PASS_REGULAR_EXPRESSION "\"name\": \"ctrace_stack_analyzer\"")
add_test(NAME ctrace_sarif_report_file_is_the_merged_log
    COMMAND sh -c "grep -q '\"\$schema\"' ${ctrace_merged_sarif} && grep -q '\"name\": \"cppcheck\"' ${ctrace_merged_sarif} && grep -q '\"name\": \"ctrace_stack_analyzer\"' ${ctrace_merged_sarif} && ! grep -q 'diagnosticsSummary' ${ctrace_merged_sarif}")
set_tests_properties(ctrace_sarif_report_file_is_the_merged_log PROPERTIES
    DEPENDS ctrace_sarif_is_merged_per_tool)

# The double free is found by cppcheck and the stack analyzer: one SARIF result names both.
set(ctrace_dedup_sarif ${CMAKE_BINARY_DIR}/ctrace-dedup.sarif)
add_test(NAME ctrace_sarif_merges_cross_tool_duplicates
    COMMAND sh -c "$<TARGET_FILE:ctrace> --sarif-format --fail-on none --invoke cppcheck,ctrace_stack_analyzer --config ${CMAKE_SOURCE_DIR}/config/tool-config.json --input ${CMAKE_SOURCE_DIR}/tests/double_free.c --report-file ${ctrace_dedup_sarif} > /dev/null && test $(grep -c 'coretrace/v1' ${ctrace_dedup_sarif}) -eq 1 && grep -q 'alsoReportedBy' ${ctrace_dedup_sarif}")

# Every tool spells a file the same way: the input as typed. The stack analyzer wrote some of
# its findings (null dereference, signed overflow) with an absolute path.
add_test(NAME ctrace_stack_analyzer_paths_follow_the_input
    COMMAND sh -c "$<TARGET_FILE:ctrace> --sarif-format --fail-on none --invoke ctrace_stack_analyzer,cppcheck --input tests/null_pointer.c,tests/dead_code.cc > ${CMAKE_BINARY_DIR}/paths.sarif 2>/dev/null && grep -q '\"uri\": \"tests/null_pointer.c\"' ${CMAKE_BINARY_DIR}/paths.sarif && ! grep -q '\"uri\": \"/' ${CMAKE_BINARY_DIR}/paths.sarif"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Every stack analyzer finding names its rule and CWE (coretrace-stack-analyzer >= v0.21.0):
# the strcpy overflow is MemcpyWithStackDest, CWE-120, the CWE flawfinder gives it too.
add_test(NAME ctrace_stack_analyzer_findings_name_rule_and_cwe
    COMMAND sh -c "$<TARGET_FILE:ctrace> --sarif-format --fail-on none --invoke ctrace_stack_analyzer --input ${CMAKE_SOURCE_DIR}/tests/buffer_overflow.cc > ${CMAKE_BINARY_DIR}/rule-cwe.sarif 2>/dev/null && grep -q '\"ruleId\": \"MemcpyWithStackDest\"' ${CMAKE_BINARY_DIR}/rule-cwe.sarif && grep -q '\"cwe\": \"CWE-120\"' ${CMAKE_BINARY_DIR}/rule-cwe.sarif")

# Recursion findings name their rule too (coretrace-stack-analyzer >= v0.22.0): the
# unbounded self call is Recursion.Unconditional, CWE-674, and no finding is rule None.
add_test(NAME ctrace_stack_analyzer_recursion_has_a_rule
    COMMAND sh -c "$<TARGET_FILE:ctrace> --sarif-format --fail-on none --invoke ctrace_stack_analyzer --input ${CMAKE_SOURCE_DIR}/tests/infinite_recursion.c > ${CMAKE_BINARY_DIR}/recursion.sarif 2>/dev/null && grep -q '\"ruleId\": \"Recursion.Unconditional\"' ${CMAKE_BINARY_DIR}/recursion.sarif && grep -q '\"cwe\": \"CWE-674\"' ${CMAKE_BINARY_DIR}/recursion.sarif && ! grep -q '\"ruleId\": \"None\"' ${CMAKE_BINARY_DIR}/recursion.sarif")

# The CI's network steps retry with a growing delay (apt.llvm.org drops connections).
add_test(NAME ctrace_ci_retry_helper COMMAND sh ${CMAKE_SOURCE_DIR}/tests/retry_test.sh)

# Python sources go to the Python analyzer only; the C/C++ tools never see them.
add_test(NAME ctrace_python_file_skips_c_tools
    COMMAND sh -c "$<TARGET_FILE:ctrace> --input ${CMAKE_SOURCE_DIR}/tests/python/project/app/main.py --invoke cppcheck,ctrace_stack_analyzer; test $? -eq 0")
set_tests_properties(ctrace_python_file_skips_c_tools PROPERTIES
    FAIL_REGULAR_EXPRESSION "Running cppcheck on|Running CoreTrace Stack Analyzer on")

# End to end through the CLI with a stand-in for the analyzer (the real one needs Nuitka, see
# ENABLE_PYTHON_ANALYZER): findings are read from its SARIF and counted.
file(WRITE ${CMAKE_BINARY_DIR}/fake-python-analyzer.json
    "{\"schema_version\": 1, \"tools\": {\"coretrace-python-analyzer\": {\"path\": \"${CMAKE_SOURCE_DIR}/tests/fake-python-analyzer.sh\"}}}\n")
add_test(NAME ctrace_python_analyzer_findings_are_counted
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --config ${CMAKE_BINARY_DIR}/fake-python-analyzer.json --input ${CMAKE_SOURCE_DIR}/tests/python/project/app/main.py,${CMAKE_SOURCE_DIR}/tests/double_free.c --invoke coretrace-python-analyzer 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 2 && printf '%s\\n' \"$out\" | grep -q 'Running coretrace-python-analyzer on .* for 1 input file(s)' && test $(printf '%s\\n' \"$out\" | grep -c 'Running coretrace-python-analyzer on') -eq 1")
add_test(NAME ctrace_python_analyzer_summary
    COMMAND ctrace --fail-on none --config ${CMAKE_BINARY_DIR}/fake-python-analyzer.json
        --input ${CMAKE_SOURCE_DIR}/tests/python/project/app/main.py --invoke coretrace-python-analyzer)
set_tests_properties(ctrace_python_analyzer_summary PROPERTIES
    PASS_REGULAR_EXPRESSION "\\(coretrace-python-analyzer\\) Diagnostics summary: info=0, warning=0, error=2")
# Two inputs of one project: a single analyzer run, and both inputs' findings.
add_test(NAME ctrace_python_analyzer_runs_once_per_project
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --fail-on none --config ${CMAKE_BINARY_DIR}/fake-python-analyzer.json --input ${CMAKE_SOURCE_DIR}/tests/python/project/app/main.py,${CMAKE_SOURCE_DIR}/tests/python/project/app/other.py --invoke coretrace-python-analyzer 2>&1); printf '%s\\n' \"$out\"; test $(printf '%s\\n' \"$out\" | grep -c 'Running coretrace-python-analyzer on') -eq 1 && printf '%s\\n' \"$out\" | grep -q 'Diagnostics summary: info=0, warning=0, error=3'")

# C and Python in one run: one SARIF log, one run per tool, each finding in a file of its
# tool's language.
add_executable(ctrace_mixed_language_tests tests/mixed_language_tests.cpp)
target_link_libraries(ctrace_mixed_language_tests
    PRIVATE nlohmann_json::nlohmann_json coretrace::logger)
add_test(NAME ctrace_mixed_language_tests
    COMMAND ctrace_mixed_language_tests $<TARGET_FILE:ctrace> ${CMAKE_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/fake-python-analyzer.json ${CMAKE_BINARY_DIR})

# A compilation database drives the inputs end to end: each source is resolved against its
# entry's directory, and a discovered database's dependency entries (_deps) are left out
# unless --include-compdb-deps asks for them.
set(ctrace_compdb_fixture ${CMAKE_BINARY_DIR}/compdb-fixture)
file(WRITE ${ctrace_compdb_fixture}/_deps/dep/dep.c "int dep(void) { return 0; }\n")
file(WRITE ${ctrace_compdb_fixture}/compile_commands.json
    "[\n  {\"directory\": \"${CMAKE_SOURCE_DIR}/tests\", \"file\": \"double_free.c\", \"command\": \"cc -c double_free.c\"},\n  {\"directory\": \"${ctrace_compdb_fixture}\", \"file\": \"_deps/dep/dep.c\", \"command\": \"cc -c _deps/dep/dep.c\"}\n]\n")
add_test(NAME ctrace_compile_commands_drive_the_inputs
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --fail-on none --invoke cppcheck --compile-commands ${ctrace_compdb_fixture} 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 0 && printf '%s\\n' \"$out\" | grep -q 'Running cppcheck on .*tests/double_free.c' && ! printf '%s\\n' \"$out\" | grep -q '_deps/dep/dep.c'")
add_test(NAME ctrace_compile_commands_include_deps_on_request
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --fail-on none --invoke cppcheck --compile-commands ${ctrace_compdb_fixture} --include-compdb-deps 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 0 && printf '%s\\n' \"$out\" | grep -q 'Running cppcheck on .*tests/double_free.c' && printf '%s\\n' \"$out\" | grep -q 'Running cppcheck on .*_deps/dep/dep.c'")

# --static runs every static tool on the files of its language. flawfinder, TscanCode and IKOS
# are pointed at missing commands, so that the result does not depend on what is installed:
# they are reported and the analysis is incomplete (exit 3), while cppcheck and the stack
# analyzer analyze the C file and the Python analyzer the Python one.
file(WRITE ${CMAKE_BINARY_DIR}/static-tools.json
    "{\"schema_version\": 1, \"tools\": {\"coretrace-python-analyzer\": {\"path\": \"${CMAKE_SOURCE_DIR}/tests/fake-python-analyzer.sh\"}, \"flawfinder\": {\"path\": \"/nonexistent/ctrace-flawfinder\"}, \"tscancode\": {\"path\": \"/nonexistent/ctrace-tscancode\"}, \"ikos\": {\"path\": \"/nonexistent/ctrace-ikos\"}}}\n")
add_test(NAME ctrace_static_runs_every_static_tool
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --static --config ${CMAKE_BINARY_DIR}/static-tools.json --input ${CMAKE_SOURCE_DIR}/tests/double_free.c,${CMAKE_SOURCE_DIR}/tests/python/project/app/main.py 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 3 && printf '%s\\n' \"$out\" | grep -q 'Running static analysis on 2 file(s)' && printf '%s\\n' \"$out\" | grep -q 'Running cppcheck on .*double_free.c' && printf '%s\\n' \"$out\" | grep -q '(ctrace_stack_analyzer) Diagnostics summary: info=0, warning=0, error=1' && printf '%s\\n' \"$out\" | grep -q 'Running coretrace-python-analyzer on .* for 1 input file(s)' && printf '%s\\n' \"$out\" | grep -q 'Analysis incomplete: flawfinder,ikos,tscancode could not run' && ! printf '%s\\n' \"$out\" | grep -q 'Running cppcheck on .*main.py'")
# --dyn runs the dynamic tool slots. They are placeholders until the runtime analyzer takes
# them (#53, #112): they run without failing and report nothing.
add_test(NAME ctrace_dyn_runs_the_dynamic_tools
    COMMAND sh -c "out=$($<TARGET_FILE:ctrace> --dyn --input ${CMAKE_SOURCE_DIR}/tests/double_free.c 2>&1); status=$?; printf '%s\\n' \"$out\"; test $status -eq 0 && printf '%s\\n' \"$out\" | grep -q 'Running dynamic analysis on 1 file(s)' && test $(printf '%s\\n' \"$out\" | grep -c 'Running dyn_tools_[123] on') -eq 3")

# A bundled tool is found next to ctrace, from a copy of the release layout run elsewhere with
# an empty environment. The stand-in covers the lookup everywhere; the real standalone
# analyzer covers "no Python needed" when it is built.
add_test(NAME ctrace_bundled_tool_is_found_next_to_ctrace
    COMMAND sh ${CMAKE_SOURCE_DIR}/tests/python/run-relocated.sh $<TARGET_FILE:ctrace>
        ${CMAKE_SOURCE_DIR}/tests/fake-python-analyzer.sh ${CMAKE_BINARY_DIR}/relocated-fake)
if(ENABLE_PYTHON_ANALYZER)
    add_test(NAME ctrace_python_analyzer_standalone_runs_anywhere
        COMMAND sh ${CMAKE_SOURCE_DIR}/tests/python/run-relocated.sh $<TARGET_FILE:ctrace>
            ${CORETRACE_PYTHON_ANALYZER_DIR} ${CMAKE_BINARY_DIR}/relocated-python-analyzer)
    # What `cmake --install` produces is what the release archive ships.
    set(ctrace_install_check ${CMAKE_BINARY_DIR}/install-check)
    add_test(NAME ctrace_install_ships_python_analyzer
        COMMAND sh -c "rm -rf '${ctrace_install_check}' && '${CMAKE_COMMAND}' --install '${CMAKE_BINARY_DIR}' --prefix '${ctrace_install_check}' > /dev/null && sh '${CMAKE_SOURCE_DIR}/tests/python/check-installed.sh' '${ctrace_install_check}' '${ctrace_install_check}-work'")
    # The shipped analyzer's rules reach the report: code-injection is one of its 0.11.0.
    add_test(NAME ctrace_python_analyzer_reports_code_injection
        COMMAND ctrace --fail-on none
            --input ${CMAKE_SOURCE_DIR}/tests/python/code-injection/calculator.py
            --invoke coretrace-python-analyzer)
    set_tests_properties(ctrace_python_analyzer_reports_code_injection PROPERTIES
        PASS_REGULAR_EXPRESSION "calculator\\.py:4:[0-9]+: error: .*\\[coretrace-python-analyzer/code-injection\\]")
endif()

# `ctest -L unit` runs the tests of one component in process; `ctest -L integration` those that
# run ctrace, a server, a socket peer or a real analysis tool. A test not listed here is an
# integration test.
set(coretrace_unit_tests
    ctrace_config_tests
    ctrace_tool_arguments_tests
    ctrace_files_tests
    ctrace_tool_output_tests
    ctrace_report_file_tests
    ctrace_tool_diagnostics_tests
    ctrace_tool_routing_tests
    ctrace_exit_policy_tests
    ctrace_process_tests
    ctrace_ipc_socket_tests
    ctrace_ci_retry_helper
)
get_property(coretrace_tests DIRECTORY PROPERTY TESTS)
foreach(coretrace_test IN LISTS coretrace_tests)
    if(coretrace_test IN_LIST coretrace_unit_tests)
        set_tests_properties(${coretrace_test} PROPERTIES LABELS unit)
    else()
        set_tests_properties(${coretrace_test} PROPERTIES LABELS integration)
    endif()
endforeach()

# Applied last so every executable we own exists by now.
foreach(coretrace_target
    ctrace
    ctrace_config_tests
    ctrace_tool_arguments_tests
    ctrace_files_tests
    ctrace_tool_output_tests
    ctrace_report_file_tests
    ctrace_tool_diagnostics_tests
    ctrace_tool_routing_tests
    ctrace_exit_policy_tests
    ctrace_process_tests
    ctrace_ipc_socket_tests
    ctrace_server_concurrency_tests
    ctrace_server_http_tests
    ctrace_server_cli_tests
    ctrace_mixed_language_tests
    ctrace_stack_analyzer_bridge_tests
)
    coretrace_enable_warnings(${coretrace_target})
    coretrace_enable_coverage(${coretrace_target})
endforeach()
