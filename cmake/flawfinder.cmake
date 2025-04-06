include(ExternalProject)

ExternalProject_Add(
    flawfinder
    GIT_REPOSITORY https://github.com/david-a-wheeler/flawfinder.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/flawfinder
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
        COMMAND cd ${CMAKE_BINARY_DIR}/flawfinder/src/flawfinder
        COMMAND cp ${CMAKE_BINARY_DIR}/flawfinder/src/flawfinder/flawfinder.py .
    INSTALL_COMMAND ""
)
