# SPDX-License-Identifier: Apache-2.0
#
# Links coretrace-concurrency-analyzer into ctrace, the way the stack analyzer is: it runs in
# process, so nothing ships beside ctrace. Both analyzers compile their inputs through
# coretrace-compiler, which each fetches under the name `cc`. FetchContent keeps the first
# declaration of a name: a pin that differed from the stack analyzer's would be replaced by it
# silently, and the concurrency analyzer would build against a compiler library it was never
# validated with. The two pins are compared instead, and a mismatch stops the configuration.
include(FetchContent)

FetchContent_Declare(
  concurrency_analyzer
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-concurrency-analyzer.git
  GIT_TAG v0.7.1
  EXCLUDE_FROM_ALL
)

# Only the library, static: one copy of the compiler library then links into ctrace, shared
# with the stack analyzer, and nothing ships beside it. Both options are normal variables for the
# analyzer's scope only, as the stack analyzer's options of the same names leave ON in the cache.
set(BUILD_CLI OFF)
set(BUILD_SHARED_LIB OFF)
FetchContent_MakeAvailable(concurrency_analyzer)
unset(BUILD_CLI)
unset(BUILD_SHARED_LIB)

# The commit an analyzer's cmake/compiler/coretrace-compiler.cmake fetches `cc` at.
function(coretrace_compiler_pin analyzer_source_dir out_var)
    set(pin_file "${analyzer_source_dir}/cmake/compiler/coretrace-compiler.cmake")
    if(NOT EXISTS "${pin_file}")
        message(FATAL_ERROR "No coretrace-compiler pin at ${pin_file}: the analyzer's layout "
            "changed, review how it fetches the compiler library before moving its version.")
    endif()
    file(READ "${pin_file}" pin_text)
    if(NOT pin_text MATCHES "GIT_TAG[ \t]+([0-9a-fA-F]+)")
        message(FATAL_ERROR "No GIT_TAG commit in ${pin_file}: review how the analyzer fetches "
            "the compiler library before moving its version.")
    endif()
    set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

coretrace_compiler_pin("${stack_analyzer_SOURCE_DIR}" coretrace_stack_analyzer_cc_pin)
coretrace_compiler_pin("${concurrency_analyzer_SOURCE_DIR}" coretrace_concurrency_analyzer_cc_pin)
if(NOT coretrace_stack_analyzer_cc_pin STREQUAL coretrace_concurrency_analyzer_cc_pin)
    message(FATAL_ERROR "coretrace-stack-analyzer pins coretrace-compiler at "
        "${coretrace_stack_analyzer_cc_pin} and coretrace-concurrency-analyzer at "
        "${coretrace_concurrency_analyzer_cc_pin}. One copy of the compiler library links into "
        "ctrace, so the two analyzers must agree: move one of the pins in "
        "cmake/stackUsageAnalyzer.cmake and cmake/concurrencyAnalyzer.cmake to a release built "
        "on the other's commit.")
endif()
