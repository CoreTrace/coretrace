# SPDX-License-Identifier: Apache-2.0
#
# Ships coretrace-runtime-analyzer next to ctrace. Its release archive for this machine's
# architecture is downloaded, checked against the SHA-256 pinned below, and staged where ctrace
# looks for bundled tools, <build>/libexec/coretrace/coretrace-runtime-analyzer/, then installed
# to <prefix>/libexec/coretrace/coretrace-runtime-analyzer/. The archive is a tree of its own
# (bin/runtime-analyzer, its LLVM libraries, Clang's headers), run as a separate process.
#
# The hashes are this repository's: the .sha256 published next to an archive proves that the
# download is intact, not that it is the archive reviewed here. Moving to another version means
# updating the version and both hashes together.
set(CORETRACE_RUNTIME_ANALYZER_VERSION "v0.1.0")
set(coretrace_runtime_analyzer_sha256_amd64
    "6a07af5ca187d62e8513f1b0b8a0a42261297c3ad96a6e4b0a30aa31dffa47c8")
set(coretrace_runtime_analyzer_sha256_arm64
    "9fba15e2371e81941d82eac95b92d68ea2921b4708ecb4013d4c59ce1ca25448")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "ENABLE_RUNTIME_ANALYZER ships the runtime analyzer's Linux release "
        "archives. On ${CMAKE_SYSTEM_NAME}, build coretrace-runtime-analyzer and point "
        "tools.coretrace-runtime-analyzer.path at it instead.")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" coretrace_runtime_analyzer_cpu)
if(coretrace_runtime_analyzer_cpu MATCHES "^(x86_64|amd64)$")
    set(coretrace_runtime_analyzer_arch amd64)
elseif(coretrace_runtime_analyzer_cpu MATCHES "^(aarch64|arm64)$")
    set(coretrace_runtime_analyzer_arch arm64)
else()
    message(FATAL_ERROR "No coretrace-runtime-analyzer release for ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(coretrace_runtime_analyzer_name
    "runtime-analyzer-${CORETRACE_RUNTIME_ANALYZER_VERSION}-linux-${coretrace_runtime_analyzer_arch}")
set(coretrace_runtime_analyzer_archive
    "${CMAKE_BINARY_DIR}/_downloads/${coretrace_runtime_analyzer_name}.tar.gz")
set(CORETRACE_RUNTIME_ANALYZER_DIR
    "${CMAKE_BINARY_DIR}/libexec/coretrace/coretrace-runtime-analyzer")

# An archive already downloaded with the expected hash is not fetched again.
file(DOWNLOAD
    "https://github.com/CoreTrace/coretrace-runtime-analyzer/releases/download/${CORETRACE_RUNTIME_ANALYZER_VERSION}/${coretrace_runtime_analyzer_name}.tar.gz"
    "${coretrace_runtime_analyzer_archive}"
    EXPECTED_HASH "SHA256=${coretrace_runtime_analyzer_sha256_${coretrace_runtime_analyzer_arch}}"
    TLS_VERIFY ON
    STATUS coretrace_runtime_analyzer_download)
list(GET coretrace_runtime_analyzer_download 0 coretrace_runtime_analyzer_download_code)
if(NOT coretrace_runtime_analyzer_download_code EQUAL 0)
    message(FATAL_ERROR "Cannot download ${coretrace_runtime_analyzer_name}.tar.gz: "
        "${coretrace_runtime_analyzer_download}")
endif()

set(coretrace_runtime_analyzer_extract "${CMAKE_BINARY_DIR}/_downloads/runtime-analyzer")
file(REMOVE_RECURSE "${coretrace_runtime_analyzer_extract}" "${CORETRACE_RUNTIME_ANALYZER_DIR}")
file(ARCHIVE_EXTRACT INPUT "${coretrace_runtime_analyzer_archive}"
    DESTINATION "${coretrace_runtime_analyzer_extract}")
get_filename_component(coretrace_runtime_analyzer_parent "${CORETRACE_RUNTIME_ANALYZER_DIR}"
    DIRECTORY)
file(MAKE_DIRECTORY "${coretrace_runtime_analyzer_parent}")
file(RENAME "${coretrace_runtime_analyzer_extract}/${coretrace_runtime_analyzer_name}"
    "${CORETRACE_RUNTIME_ANALYZER_DIR}")
message(STATUS "coretrace-runtime-analyzer ${CORETRACE_RUNTIME_ANALYZER_VERSION} "
    "(${coretrace_runtime_analyzer_arch}) staged in ${CORETRACE_RUNTIME_ANALYZER_DIR}")

install(DIRECTORY "${CORETRACE_RUNTIME_ANALYZER_DIR}/"
    DESTINATION libexec/coretrace/coretrace-runtime-analyzer
    USE_SOURCE_PERMISSIONS)
