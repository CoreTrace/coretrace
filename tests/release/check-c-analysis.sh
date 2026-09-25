#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Checks that an installation laid out as a release archive analyzes C without an installed
# clang: ctrace runs with an empty environment, so no clang can be found through PATH, and the
# stack analyzer must compile the source with the Clang headers shipped next to ctrace. Only the
# C library's headers (libc6-dev, glibc-devel), part of any C build environment, come from the
# system.
#
# Usage: check-c-analysis.sh <prefix> <scratch dir>
set -eu

prefix=$1
work=$2
here=$(cd "$(dirname "$0")" && pwd)

rm -rf "$work"
mkdir -p "$work"
cp "$here/../double_free.c" "$work/double_free.c"

set +e
output=$(cd "$work" && env -i PATH=/nonexistent "$prefix/bin/ctrace" \
    --invoke ctrace_stack_analyzer --input double_free.c 2>&1)
status=$?
set -e
printf '%s\n' "$output"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ "$status" -eq 2 ] || fail "expected exit code 2 (findings), got $status"
printf '%s\n' "$output" | grep -q 'potential double release' ||
    fail "the double free of double_free.c is not reported"
printf '%s\n' "$output" |
    grep -q '(ctrace_stack_analyzer) Diagnostics summary: info=0, warning=0, error=1' ||
    fail "the stack analyzer's finding is not counted"
echo "PASS: ctrace analyzes C from $prefix without an installed clang"
