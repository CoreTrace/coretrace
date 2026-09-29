#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# check-licenses.sh on small installations: a complete one passes; a library or an executable
# missing from the inventory, a license left empty, or a line that names no license, fails and
# is named.
set -u

here=$(cd "$(dirname "$0")" && pwd)
check_licenses=$here/../../scripts/release/check-licenses.sh
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
prefix=$work/prefix
failures=0

# <expected exit status> <description> <text the output must contain, or nothing>
check() {
    output=$(sh "$check_licenses" "$prefix" "$work/inventory" 2>&1)
    status=$?
    if [ "$status" -ne "$1" ] || { [ -n "$3" ] && ! printf '%s\n' "$output" | grep -qF -- "$3"; }; then
        echo "[FAIL] $2 (exit code $status)"
        printf '%s\n' "$output" | sed 's/^/    /'
        failures=$((failures + 1))
    else
        echo "[PASS] $2"
    fi
}

mkdir -p "$prefix/bin" "$prefix/lib/clang/20/include" "$prefix/libexec/tool"
printf 'x' > "$prefix/bin/app"
chmod +x "$prefix/bin/app"
printf 'x' > "$prefix/lib/libfoo.so.1.2"
ln -s libfoo.so.1.2 "$prefix/lib/libfoo.so.1"
printf 'x' > "$prefix/lib/clang/20/include/stddef.h"
printf 'x' > "$prefix/libexec/tool/_module.so"
printf 'x' > "$prefix/README.md"
printf 'license\n' > "$prefix/LICENSE"
for directory in app ubuntu/libfoo1 tool; do
    mkdir -p "$prefix/licenses/$directory"
    printf 'license\n' > "$prefix/licenses/$directory/LICENSE"
done
cat > "$work/inventory" <<'INVENTORY'
# A comment, then a blank line.

bin/app                  LICENSE licenses/app
*lib/libfoo.so.*         licenses/ubuntu/libfoo1
lib/clang/*/include/*    licenses/ubuntu/libfoo1
libexec/tool/*.so        licenses/tool
INVENTORY

check 0 "every library and executable is listed, with its license" ""

printf 'x' > "$prefix/lib/libbar.so.2"
check 1 "a library missing from the inventory fails and is named" "lib/libbar.so.2"
rm "$prefix/lib/libbar.so.2"

printf 'x' > "$prefix/libexec/tool/helper"
chmod +x "$prefix/libexec/tool/helper"
check 1 "an executable missing from the inventory fails and is named" "libexec/tool/helper"
rm "$prefix/libexec/tool/helper"

rm "$prefix/licenses/tool/LICENSE"
check 1 "a license directory left empty fails and is named" "licenses/tool"
printf 'license\n' > "$prefix/licenses/tool/LICENSE"

rm "$prefix/LICENSE"
check 1 "a license file that is missing fails and is named" "LICENSE"
printf 'license\n' > "$prefix/LICENSE"

mkdir -p "$prefix/libexec/tool/licenses"
printf 'x' > "$prefix/libexec/tool/licenses/libnotice.so"
check 0 "files under a licenses directory are not classified" ""
rm -r "$prefix/libexec/tool/licenses"

rm "$prefix/lib/clang/20/include/stddef.h"
rm -r "$prefix/licenses/ubuntu/libfoo1"
printf 'x' > "$prefix/lib/clang/20/include/stddef.h"
check 1 "a file that is neither a library nor an executable still needs the license its line names" "licenses/ubuntu/libfoo1"
mkdir -p "$prefix/licenses/ubuntu/libfoo1"
printf 'license\n' > "$prefix/licenses/ubuntu/libfoo1/LICENSE"

# The inventory can list what only another architecture or configuration ships.
printf 'lib/libunused.so.*    licenses/not-shipped\n' >> "$work/inventory"
check 0 "a line that matches no file needs no license" ""

printf 'libexec/tool/*.so\n' > "$work/bare-line"
cat "$work/bare-line" "$work/inventory" > "$work/inventory.new" && mv "$work/inventory.new" "$work/inventory"
check 1 "a line that names no license fails and is named" "libexec/tool/*.so"

if [ "$failures" -ne 0 ]; then
    echo "$failures check-licenses test(s) failed"
    exit 1
fi
echo "check-licenses tests: all passed"
