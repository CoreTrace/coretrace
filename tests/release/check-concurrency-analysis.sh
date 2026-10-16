#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Checks that an installation laid out as a release archive finds a data race with the
# concurrency analyzer linked into ctrace: no clang installed (an empty environment, as for
# check-c-analysis.sh), only the C library's headers, which pthread.h is part of. On macOS they
# are in the SDK, found through xcrun with the system's tools in PATH, as in check-c-analysis.sh.
#
# Usage: check-concurrency-analysis.sh <prefix> <scratch dir>
set -eu

prefix=$1
work=$2
here=$(cd "$(dirname "$0")" && pwd)
case $(uname -s) in
    Darwin) system_path=/usr/bin:/bin ;;
    *) system_path=/nonexistent ;;
esac

rm -rf "$work"
mkdir -p "$work"
cp "$here/../concurrency/race.c" "$work/race.c"

set +e
output=$(cd "$work" && env -i PATH="$system_path" "$prefix/bin/ctrace" \
    --invoke coretrace-concurrency-analyzer --input race.c 2>&1)
status=$?
set -e
printf '%s\n' "$output"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ "$status" -eq 2 ] || fail "expected exit code 2 (findings), got $status"
printf '%s\n' "$output" | grep -q \
    '^race.c:14:[0-9]*: error: .*\[coretrace-concurrency-analyzer/DataRaceGlobal\]$' ||
    fail "the data race of race.c is not reported"
printf '%s\n' "$output" |
    grep -q '(coretrace-concurrency-analyzer) Diagnostics summary: info=0, warning=0, error=1' ||
    fail "the concurrency analyzer's finding is not counted"
echo "PASS: ctrace finds the data race from $prefix with the linked concurrency analyzer"
