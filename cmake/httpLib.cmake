include(FetchContent)

FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(cpp_httplib)
