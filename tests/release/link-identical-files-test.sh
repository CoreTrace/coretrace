#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# link-identical-files.sh on a small tree: a file identical to the one at the same path of the
# reference becomes a hard link to it; a file that differs, a file only the tree has, and a
# symbolic link on either side, keep their own bytes. Running it again changes nothing.
set -u

here=$(cd "$(dirname "$0")" && pwd)
link=$here/../../scripts/release/link-identical-files.sh
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0

inode() {
    ls -i "$1" | awk '{ print $1 }'
}

expect() {
    if eval "$1"; then
        echo "[PASS] $2"
    else
        echo "[FAIL] $2"
        failures=$((failures + 1))
    fi
}

reference=$work/prefix/lib
tree=$work/prefix/libexec/tool/lib
mkdir -p "$reference/clang/20/include" "$tree/clang/20/include"
printf 'shared library\n' > "$reference/libshared.so.1"
cp "$reference/libshared.so.1" "$tree/libshared.so.1"
printf 'header\n' > "$reference/clang/20/include/stddef.h"
cp "$reference/clang/20/include/stddef.h" "$tree/clang/20/include/stddef.h"
printf 'the reference build\n' > "$reference/libdiffers.so.2"
printf 'the tool build\n' > "$tree/libdiffers.so.2"
printf 'only the tool has it\n' > "$tree/libown.so"
printf 'same bytes\n' > "$reference/libtarget.so.3"
ln -s libtarget.so.3 "$reference/liblinked.so"
printf 'same bytes\n' > "$tree/liblinked.so"
printf 'same bytes\n' > "$tree/libtarget.so.3"
ln -s libtarget.so.3 "$tree/libalias.so"
cp "$reference/libtarget.so.3" "$reference/libalias.so"

output=$(sh "$link" "$reference" "$tree" 2>&1)
status=$?
printf '%s\n' "$output" | sed 's/^/    /'
expect '[ "$status" -eq 0 ]' "the run succeeds"
expect '[ "$(inode "$tree/libshared.so.1")" = "$(inode "$reference/libshared.so.1")" ]' \
    "an identical library becomes a hard link to the reference's"
expect '[ "$(inode "$tree/clang/20/include/stddef.h")" = "$(inode "$reference/clang/20/include/stddef.h")" ]' \
    "an identical file in a subdirectory becomes a hard link too"
expect 'grep -qx "shared library" "$tree/libshared.so.1"' "a linked file keeps its content"
expect '[ "$(inode "$tree/libdiffers.so.2")" != "$(inode "$reference/libdiffers.so.2")" ] && grep -qx "the tool build" "$tree/libdiffers.so.2"' \
    "a file that differs keeps its own bytes"
expect 'grep -qx "only the tool has it" "$tree/libown.so"' "a file only the tree has is left alone"
expect '[ ! -L "$tree/liblinked.so" ] && [ "$(inode "$tree/liblinked.so")" != "$(inode "$reference/libtarget.so.3")" ]' \
    "a file whose reference is a symbolic link is left alone"
expect '[ -L "$tree/libalias.so" ]' "a symbolic link of the tree stays a symbolic link"
expect 'printf "%s\n" "$output" | grep -q "linked 3 files"' "the run reports how many files it linked"

again=$(sh "$link" "$reference" "$tree" 2>&1)
expect '[ "$(inode "$tree/libshared.so.1")" = "$(inode "$reference/libshared.so.1")" ] && grep -qx "shared library" "$reference/libshared.so.1"' \
    "running it again keeps the links and the content"

if [ "$failures" -ne 0 ]; then
    echo "$failures link-identical-files test(s) failed"
    exit 1
fi
echo "link-identical-files tests: all passed"
