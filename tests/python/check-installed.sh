#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Checks a CoreTrace installation laid out as a release archive (<prefix>/bin/ctrace and
# <prefix>/libexec/coretrace/coretrace-python-analyzer/) from a user's point of view: ctrace
# runs with an empty environment, so the analyzer can only be the one shipped next to it, and
# it can only use what it ships with: no Python, no packages.
#
# Usage: check-installed.sh <prefix> <scratch dir>
set -eu

prefix=$1
work=$2
here=$(cd "$(dirname "$0")" && pwd)

rm -rf "$work"
mkdir -p "$work"
cp "$here/vulnerable.py" "$work/input.py"

set +e
output=$(cd "$work" && env -i PATH=/nonexistent "$prefix/bin/ctrace" \
    --invoke coretrace-python-analyzer --input input.py 2>&1)
status=$?
set -e
printf '%s\n' "$output"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ "$status" -eq 2 ] || fail "expected exit code 2 (findings), got $status"
printf '%s\n' "$output" |
    grep -q '^input.py:6:12: error: .*\[coretrace-python-analyzer/dangerous-eval\]$' ||
    fail "the dangerous-eval finding of input.py:6:12 is not reported"
printf '%s\n' "$output" |
    grep -q '(coretrace-python-analyzer) Diagnostics summary: info=0, warning=0, error=1' ||
    fail "the finding suppressed in source must not be counted"
echo "PASS: coretrace-python-analyzer runs from $prefix without PATH or Python"
