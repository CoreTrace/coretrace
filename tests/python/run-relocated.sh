#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Proves that a release layout works anywhere: ctrace and a bundled coretrace-python-analyzer
# are copied to a fresh directory and run with an empty environment. With no PATH, the tool can
# only be found next to ctrace, and it can only use what it ships with: no Python, no packages.
#
# Usage: run-relocated.sh <ctrace> <analyzer directory, or a single executable> <scratch dir>
set -eu

ctrace=$1
analyzer=$2
root=$3
here=$(cd "$(dirname "$0")" && pwd)
tool_dir="$root/libexec/coretrace/coretrace-python-analyzer"

rm -rf "$root"
mkdir -p "$root/bin" "$(dirname "$tool_dir")"
cp "$ctrace" "$root/bin/ctrace"
if [ -d "$analyzer" ]; then
    cp -R "$analyzer" "$tool_dir"
else
    mkdir -p "$tool_dir"
    cp "$analyzer" "$tool_dir/coretrace-python-analyzer"
fi
cp "$here/vulnerable.py" "$root/input.py"

set +e
output=$(cd "$root" && env -i PATH=/nonexistent "$root/bin/ctrace" \
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
echo "PASS: bundled coretrace-python-analyzer runs from $root without PATH or Python"
