# SPDX-License-Identifier: Apache-2.0
set(CORETRACE_VERSION_OVERRIDE "" CACHE STRING
    "Explicit CoreTrace version string embedded in the binary")

set(CORETRACE_FALLBACK_VERSION "dev" CACHE STRING
    "Fallback CoreTrace version string when Git metadata is unavailable")

function(coretrace_resolve_version out_var out_source_var)
    if(CORETRACE_VERSION_OVERRIDE)
        set(_coretrace_version "${CORETRACE_VERSION_OVERRIDE}")
        set(_coretrace_version_source "override")
    else()
        find_package(Git QUIET)
        if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}"
                        describe --tags --dirty --always --match "v*"
                RESULT_VARIABLE _coretrace_git_result
                OUTPUT_VARIABLE _coretrace_git_describe
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()

        if(DEFINED _coretrace_git_result AND _coretrace_git_result EQUAL 0
           AND NOT _coretrace_git_describe STREQUAL "")
            set(_coretrace_version "${_coretrace_git_describe}")
            set(_coretrace_version_source "git-describe")
        else()
            set(_coretrace_version "${CORETRACE_FALLBACK_VERSION}")
            set(_coretrace_version_source "fallback")
        endif()
    endif()

    set(${out_var} "${_coretrace_version}" PARENT_SCOPE)
    set(${out_source_var} "${_coretrace_version_source}" PARENT_SCOPE)
endfunction()
