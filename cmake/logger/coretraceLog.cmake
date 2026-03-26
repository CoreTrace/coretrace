set(CORETRACE_LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "Disable logger examples" FORCE)
set(CORETRACE_LOGGER_BUILD_TESTS OFF CACHE BOOL "Disable logger tests" FORCE)

include(FetchContent)

FetchContent_Declare(coretrace-logger
  GIT_REPOSITORY https://github.com/CoreTrace/coretrace-log.git
  GIT_TAG        main
)
FetchContent_GetProperties(coretrace-logger)
if(NOT coretrace-logger_POPULATED)
    FetchContent_Populate(coretrace-logger)
    add_subdirectory(${coretrace-logger_SOURCE_DIR} ${coretrace-logger_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
