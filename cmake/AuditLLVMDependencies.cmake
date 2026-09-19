# SPDX-License-Identifier: Apache-2.0
# Run on the actual artifact, including transitive shared dependencies.
cmake_minimum_required(VERSION 3.28)
if(NOT DEFINED BINARY OR NOT EXISTS "${BINARY}" OR IS_DIRECTORY "${BINARY}")
    message(FATAL_ERROR "Pass -DBINARY=<existing executable> to audit its dependencies")
endif()
if(NOT CMAKE_HOST_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
    message(FATAL_ERROR "LLVM dependency audit currently supports Linux and macOS only")
endif()

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${BINARY}"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
)
foreach(dependency IN LISTS resolved unresolved)
    get_filename_component(name "${dependency}" NAME)
    string(TOLOWER "${name}" name)
    if(name MATCHES "^(lib)?(llvm|clang).*(\\.so|\\.dylib|\\.dll)")
        message(FATAL_ERROR "Dynamic LLVM/Clang dependency in ${BINARY}: ${dependency}")
    endif()
endforeach()
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Inspect their names first: macOS system libraries may exist only in dyld's cache.
    list(FILTER unresolved EXCLUDE REGEX "^(/System/Library/|/usr/lib/)")
endif()
if(unresolved)
    message(FATAL_ERROR "Unresolved runtime dependencies in ${BINARY}: ${unresolved}")
endif()
message(STATUS "No dynamic LLVM/Clang dependency: ${BINARY}")
message(STATUS "Other runtime dependencies (not a standalone certification): ${resolved}")
