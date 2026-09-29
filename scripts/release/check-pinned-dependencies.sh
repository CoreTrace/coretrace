#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Checks that the CMake modules pin what they fetch: a git reference must be a version tag or a
# full commit, never a branch, and a download must carry its hash. A branch puts whatever it
# holds at build time into a release, unreviewed, and the release cannot be rebuilt identically.
# The optional tools that no release ships (flawfinder, TscanCode, IKOS) are left out.
#
# Usage: check-pinned-dependencies.sh <source dir>
set -eu

source_dir=$1
status=0
fail() {
    echo "FAIL: $1" >&2
    status=1
}

for file in $(cd "$source_dir" && find cmake -name '*.cmake' | sort); do
    case $file in
        cmake/flawfinder.cmake | cmake/tscancode.cmake | cmake/ikos.cmake) continue ;;
    esac
    path=$source_dir/$file
    # Each GIT_TAG, a variable resolved through the set() of the same file.
    for reference in $(awk '$1 == "GIT_TAG" { print $2 }' "$path"); do
        value=$reference
        case $reference in
            '${'*'}')
                name=${reference#??}
                name=${name%?}
                value=$(sed -n "s/^set($name \"\\([^\"]*\\)\".*/\\1/p" "$path" | head -n 1)
                ;;
        esac
        if ! printf '%s\n' "$value" | grep -Eq '^(v?[0-9]+(\.[0-9]+)+|[0-9a-f]{40})$'; then
            fail "$file fetches '${value:-$reference}', neither a version tag nor a full commit"
        fi
    done
    if grep -Eq '^[[:space:]]*URL[[:space:]]' "$path" && ! grep -q 'URL_HASH' "$path"; then
        fail "$file downloads a URL without URL_HASH"
    fi
    if grep -q 'file(DOWNLOAD' "$path" && ! grep -q 'EXPECTED_HASH' "$path"; then
        fail "$file downloads a file without EXPECTED_HASH"
    fi
done
if [ "$status" -eq 0 ]; then
    echo "PASS: every dependency the CMake modules fetch is pinned"
fi
exit "$status"
