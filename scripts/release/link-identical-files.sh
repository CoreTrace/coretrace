#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Replaces each file of <tree> whose bytes equal those of the file at the same relative path under
# <reference> by a hard link to that file. The runtime analyzer ships its own copies of the LLVM
# libraries, Clang's headers and the system libraries that ctrace ships in lib/: when both come from
# the same packages, the copies are identical, and linking them stores them once, on disk and in
# the release archive, where tar records the second path as a link. Every path stays in place, so
# each program loads its libraries as before. A file that differs, a file only <tree> has, and a
# symbolic link on either side, are left as they are: each tool keeps exactly the bytes it shipped.
# Where the file system refuses a hard link, the copy stays.
#
# Run it once the files have their final bytes: after patchelf.
#
# Usage: link-identical-files.sh <reference dir> <tree>
set -eu

reference=$1
tree=$2

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ -d "$reference" ] || fail "no directory $reference"
[ -d "$tree" ] || fail "no directory $tree"

list=$(mktemp)
trap 'rm -f "$list"' EXIT
(cd "$tree" && find . -type f -print) > "$list"

linked=0
bytes=0
while IFS= read -r relative; do
    relative=${relative#./}
    copy=$tree/$relative
    original=$reference/$relative
    if [ ! -f "$original" ] || [ -L "$original" ] || ! cmp -s "$copy" "$original"; then
        continue
    fi
    size=$(wc -c < "$copy")
    # Linked under a temporary name, then renamed over the copy: the copy is replaced in one step,
    # or stays when no link can be made.
    if ln "$original" "$copy.link.$$" 2>/dev/null; then
        mv -f "$copy.link.$$" "$copy"
        linked=$((linked + 1))
        bytes=$((bytes + size))
    fi
done < "$list"
echo "linked $linked files of $tree to identical files of $reference ($((bytes / 1048576)) MiB stored once)"
