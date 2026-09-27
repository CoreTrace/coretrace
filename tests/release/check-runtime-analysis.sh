#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Checks that an installation laid out as a release archive runs dynamic analysis with the
# runtime analyzer shipped next to ctrace: no LLVM and no clang installed, only the C++ build
# environment that links an instrumented program (g++ on Debian and Ubuntu, gcc-c++ on RHEL),
# found through the system PATH as for any build.
#
# Usage: check-runtime-analysis.sh <prefix> <scratch dir>
set -eu

prefix=$1
work=$2
here=$(cd "$(dirname "$0")" && pwd)

rm -rf "$work"
mkdir -p "$work"
cp "$here/../runtime/heap_overflow.c" "$work/heap_overflow.c"

set +e
output=$(cd "$work" && env -i PATH=/usr/bin:/bin "$prefix/bin/ctrace" \
    --dyn --input heap_overflow.c 2>&1)
status=$?
set -e
printf '%s\n' "$output"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ "$status" -eq 2 ] || fail "expected exit code 2 (findings), got $status"
printf '%s\n' "$output" | grep -q \
    '^heap_overflow.c:7:[0-9]*: error: heap-buffer-overflow WRITE of size 4 \[coretrace-runtime-analyzer/heap-buffer-overflow\]$' ||
    fail "the heap overflow of heap_overflow.c is not reported"
printf '%s\n' "$output" |
    grep -q '(coretrace-runtime-analyzer) Diagnostics summary: info=0, warning=0, error=1' ||
    fail "the runtime analyzer's finding is not counted"
echo "PASS: ctrace runs dynamic analysis from $prefix with the bundled runtime analyzer"
