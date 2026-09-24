#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Checks a CoreTrace installation laid out as a release archive (<prefix>/bin/ctrace and
# <prefix>/libexec/coretrace/coretrace-python-analyzer/) from a user's point of view: ctrace
# runs with an empty environment, so the analyzer can only be the one shipped next to it, and
# it can only use what it ships with: no Python, no packages. The input is one file of a small
# project whose flaw crosses two modules, so only a whole-project analysis reports it.
#
# Usage: check-installed.sh <prefix> <scratch dir>
set -eu

prefix=$1
work=$2
here=$(cd "$(dirname "$0")" && pwd)

rm -rf "$work"
mkdir -p "$work"
cp -R "$here/project" "$work/project"

set +e
output=$(cd "$work" && env -i PATH=/nonexistent "$prefix/bin/ctrace" \
    --invoke coretrace-python-analyzer --input project/app/main.py 2>&1)
status=$?
set -e
printf '%s\n' "$output"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ "$status" -eq 2 ] || fail "expected exit code 2 (findings), got $status"
printf '%s\n' "$output" |
    grep -q '^project/app/main.py:7:5: error: .*through app.helpers.execute \[coretrace-python-analyzer/command-injection\]$' ||
    fail "the cross-module command injection of project/app/main.py:7:5 is not reported"
if printf '%s\n' "$output" | grep -q 'app/other.py'; then
    fail "app/other.py is not an input: its findings must not be reported"
fi
printf '%s\n' "$output" |
    grep -q '^project/requirements.txt:2:1: error: CVE-2020-1747: .*\[coretrace-python-analyzer/vulnerable-dependency\]$' ||
    fail "the project's vulnerable pin in project/requirements.txt:2:1 is not reported"
printf '%s\n' "$output" |
    grep -q '(coretrace-python-analyzer) Diagnostics summary: info=0, warning=0, error=2' ||
    fail "only the input's active finding and the project's pin must be counted"
echo "PASS: coretrace-python-analyzer runs from $prefix without PATH or Python"
