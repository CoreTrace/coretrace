include(FetchContent)

FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)

FetchContent_GetProperties(cpp_httplib)
if(NOT cpp_httplib_POPULATED)
    FetchContent_Populate(cpp_httplib)
    add_subdirectory(${cpp_httplib_SOURCE_DIR} ${cpp_httplib_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
