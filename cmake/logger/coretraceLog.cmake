# SPDX-License-Identifier: Apache-2.0
set(CORETRACE_LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "Disable logger examples" FORCE)
set(CORETRACE_LOGGER_BUILD_TESTS OFF CACHE BOOL "Disable logger tests" FORCE)

include(FetchContent)

FetchContent_Declare(coretrace-logger
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-log.git
  GIT_TAG        main
  EXCLUDE_FROM_ALL
)

FetchContent_GetProperties(coretrace-logger)
if(NOT coretrace-logger_POPULATED)
  FetchContent_Populate(coretrace-logger)
endif()

if(WIN32)
  file(COPY_FILE
    "${CMAKE_SOURCE_DIR}/src/ThirdParty/CoretraceLoggerWindows.cpp"
    "${coretrace-logger_SOURCE_DIR}/src/logger.cpp"
    ONLY_IF_DIFFERENT
  )
endif()

if(NOT TARGET coretrace_logger)
  FetchContent_MakeAvailable(coretrace-logger)
endif()
