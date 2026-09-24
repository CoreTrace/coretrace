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
# Linux only with a glibc at least as recent as the build machine's.
include(ExternalProject)

set(CORETRACE_PYTHON_ANALYZER_TAG "v0.4.0" CACHE STRING
    "coretrace-python-analyzer tag to build")
set(CORETRACE_PYTHON_ANALYZER_SOURCE_DIR "" CACHE PATH
    "Local coretrace-python-analyzer checkout to build instead of downloading the tag")

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m nuitka --version
    RESULT_VARIABLE coretrace_nuitka_status
    OUTPUT_QUIET ERROR_QUIET)
if(NOT coretrace_nuitka_status EQUAL 0)
    message(FATAL_ERROR "ENABLE_PYTHON_ANALYZER needs Nuitka for ${Python3_EXECUTABLE}: "
        "run '${Python3_EXECUTABLE} -m pip install nuitka' or pass -DPython3_EXECUTABLE=")
endif()

set(CORETRACE_PYTHON_ANALYZER_DIR ${CMAKE_BINARY_DIR}/libexec/coretrace/coretrace-python-analyzer)

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

# The bundled plugins are loaded from their .py files at run time, so they are copied as is
# (--include-raw-dir) next to the compiled package instead of being compiled.
ExternalProject_Add(coretrace_python_analyzer
    ${coretrace_python_analyzer_source}
    PREFIX ${CMAKE_BINARY_DIR}/coretrace-python-analyzer
    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        ${Python3_EXECUTABLE} -m nuitka
            --mode=standalone
            --assume-yes-for-downloads
            --quiet
            --output-dir=<BINARY_DIR>
            --output-filename=coretrace-python-analyzer
            --include-package=coretrace_python
            --include-raw-dir=<SOURCE_DIR>/src/coretrace_python/bundled=coretrace_python/bundled
            <SOURCE_DIR>/src/coretrace_python
    INSTALL_COMMAND
        ${CMAKE_COMMAND} -E rm -rf ${CORETRACE_PYTHON_ANALYZER_DIR}
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory
            <BINARY_DIR>/coretrace_python.dist ${CORETRACE_PYTHON_ANALYZER_DIR}
)

install(DIRECTORY ${CORETRACE_PYTHON_ANALYZER_DIR}/
    DESTINATION libexec/coretrace/coretrace-python-analyzer
    USE_SOURCE_PERMISSIONS)
