include(ExternalProject)

ExternalProject_Add(
    tscancode
    GIT_REPOSITORY https://github.com/Tencent/TscanCode.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/tscancode
    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        COMMAND cd ${CMAKE_BINARY_DIR}/tscancode/src/tscancode/trunk && make
    INSTALL_COMMAND ""
)
