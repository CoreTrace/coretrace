#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Proves that a release layout works anywhere: ctrace and a bundled coretrace-python-analyzer
# are copied to a fresh directory, then checked like an installation (check-installed.sh).
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

exec sh "$here/check-installed.sh" "$root" "$root/work"
