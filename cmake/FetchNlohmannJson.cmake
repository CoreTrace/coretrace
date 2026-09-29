# SPDX-License-Identifier: Apache-2.0
include(FetchContent)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    # The SHA-256 the v3.11.3 release notes publish for json.tar.xz.
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(json)
