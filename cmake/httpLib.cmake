# SPDX-License-Identifier: Apache-2.0
include(FetchContent)

# The server speaks plain HTTP and compresses nothing: cpp-httplib must not link OpenSSL or Brotli
# because a build machine has them (Homebrew's, on macOS), as they would then ship with ctrace.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "ctrace serves plain HTTP" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "ctrace compresses no response" FORCE)

FetchContent_Declare(
    cpp_httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(cpp_httplib)
