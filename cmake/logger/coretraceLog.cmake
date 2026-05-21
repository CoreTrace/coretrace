# SPDX-License-Identifier: Apache-2.0
include_guard(GLOBAL)

if(TARGET coretrace::logger)
  return()
endif()

if(TARGET coretrace_logger)
  add_library(coretrace::logger ALIAS coretrace_logger)
  return()
endif()

set(CORETRACE_LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "Disable logger examples" FORCE)
set(CORETRACE_LOGGER_BUILD_TESTS OFF CACHE BOOL "Disable logger tests" FORCE)

include(FetchContent)

FetchContent_Declare(coretrace-logger
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-log.git
  GIT_TAG        main
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(coretrace-logger)
