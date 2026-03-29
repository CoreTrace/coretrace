# SPDX-License-Identifier: Apache-2.0
include(ExternalProject)

ExternalProject_Add(
    flawfinder
    GIT_REPOSITORY https://github.com/david-a-wheeler/flawfinder.git
    GIT_TAG master
    PREFIX ${CMAKE_BINARY_DIR}/flawfinder
    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        ${CMAKE_COMMAND} -E make_directory <BINARY_DIR>
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        <SOURCE_DIR>/flawfinder.py
        <BINARY_DIR>/flawfinder.py
    INSTALL_COMMAND ""
)
