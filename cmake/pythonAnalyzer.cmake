# SPDX-License-Identifier: Apache-2.0
#
# Builds coretrace-python-analyzer into a standalone executable with Nuitka: the Python
# interpreter, the standard library and the analyzer's plugins are compiled or copied into one
# directory, so the tool runs on machines without Python. The result is staged where ctrace
# looks for bundled tools, <build>/libexec/coretrace/coretrace-python-analyzer/, and installed
# to <prefix>/libexec/coretrace/coretrace-python-analyzer/.
#
# Needs, at build time only: Python >= 3.11 with Nuitka (pip install nuitka), a C compiler,
# and patchelf on Linux. The executable only runs on the OS and CPU it was built on, and on
# Linux only with a glibc at least as recent as the build machine's. The compilation itself is
# scripts/build-python-analyzer.sh; CORETRACE_PYTHON_ANALYZER_PREBUILT ships an executable that
# script built elsewhere (the release builds it on an older distribution than ctrace).
include(ExternalProject)

set(CORETRACE_PYTHON_ANALYZER_TAG "v0.12.0" CACHE STRING
    "coretrace-python-analyzer tag to build")
set(CORETRACE_PYTHON_ANALYZER_SOURCE_DIR "" CACHE PATH
    "Local coretrace-python-analyzer checkout to build instead of downloading the tag")

set(CORETRACE_PYTHON_ANALYZER_PREBUILT "" CACHE PATH
    "An already compiled coretrace-python-analyzer (a Nuitka .dist directory) to ship instead of building one")

set(CORETRACE_PYTHON_ANALYZER_DIR ${CMAKE_BINARY_DIR}/libexec/coretrace/coretrace-python-analyzer)

if(CORETRACE_PYTHON_ANALYZER_PREBUILT)
    # The release compiles the analyzer on an older distribution than ctrace's, so that it
    # needs an older glibc; it is only staged here.
    if(NOT EXISTS "${CORETRACE_PYTHON_ANALYZER_PREBUILT}/coretrace-python-analyzer")
        message(FATAL_ERROR "CORETRACE_PYTHON_ANALYZER_PREBUILT has no coretrace-python-analyzer "
            "executable: ${CORETRACE_PYTHON_ANALYZER_PREBUILT}")
    endif()
    add_custom_target(coretrace_python_analyzer ALL
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CORETRACE_PYTHON_ANALYZER_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CORETRACE_PYTHON_ANALYZER_PREBUILT} ${CORETRACE_PYTHON_ANALYZER_DIR}
        COMMENT "Staging the prebuilt coretrace-python-analyzer")
else()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m nuitka --version
        RESULT_VARIABLE coretrace_nuitka_status
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT coretrace_nuitka_status EQUAL 0)
        message(FATAL_ERROR "ENABLE_PYTHON_ANALYZER needs Nuitka for ${Python3_EXECUTABLE}: "
            "run '${Python3_EXECUTABLE} -m pip install nuitka' or pass -DPython3_EXECUTABLE=")
    endif()

    if(CORETRACE_PYTHON_ANALYZER_SOURCE_DIR)
        set(coretrace_python_analyzer_source
            SOURCE_DIR ${CORETRACE_PYTHON_ANALYZER_SOURCE_DIR}
            DOWNLOAD_COMMAND "")
    else()
        set(coretrace_python_analyzer_source
            GIT_REPOSITORY https://github.com/CoreTrace/coretrace-python-analyzer.git
            GIT_TAG ${CORETRACE_PYTHON_ANALYZER_TAG}
            GIT_SHALLOW TRUE)
    endif()

    ExternalProject_Add(coretrace_python_analyzer
        ${coretrace_python_analyzer_source}
        PREFIX ${CMAKE_BINARY_DIR}/coretrace-python-analyzer
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            ${CMAKE_COMMAND} -E env PYTHON=${Python3_EXECUTABLE}
                sh ${CMAKE_SOURCE_DIR}/scripts/build-python-analyzer.sh <SOURCE_DIR> <BINARY_DIR>
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E rm -rf ${CORETRACE_PYTHON_ANALYZER_DIR}
        COMMAND
            ${CMAKE_COMMAND} -E copy_directory
                <BINARY_DIR>/coretrace_python.dist ${CORETRACE_PYTHON_ANALYZER_DIR}
    )
endif()

install(DIRECTORY ${CORETRACE_PYTHON_ANALYZER_DIR}/
    DESTINATION libexec/coretrace/coretrace-python-analyzer
    USE_SOURCE_PERMISSIONS)
