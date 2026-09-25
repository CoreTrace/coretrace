#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# scripts/ci/retry.sh retries a command with a growing delay until it succeeds, and gives up
# with the command's own status after the last attempt. Network steps of the CI (apt.llvm.org)
# rely on it.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
. "$here/../scripts/ci/retry.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

# Fails twice, then succeeds: three runs, success.
flaky() {
    count=$(cat "$work/flaky" 2>/dev/null || echo 0)
    count=$((count + 1))
    echo "$count" > "$work/flaky"
    [ "$count" -ge 3 ]
}
retry 5 0 flaky 2>/dev/null || fail "a command that recovers must succeed"
[ "$(cat "$work/flaky")" -eq 3 ] || fail "expected 3 runs, got $(cat "$work/flaky")"

# Always fails: exactly the given number of attempts, then its own status.
broken() {
    count=$(cat "$work/broken" 2>/dev/null || echo 0)
    echo $((count + 1)) > "$work/broken"
    return 7
}
status=0
retry 4 0 broken 2>/dev/null || status=$?
[ "$status" -eq 7 ] || fail "expected the command's status 7, got $status"
[ "$(cat "$work/broken")" -eq 4 ] || fail "expected 4 attempts, got $(cat "$work/broken")"

echo "PASS: retry recovers from transient failures and gives up with the command's status"
