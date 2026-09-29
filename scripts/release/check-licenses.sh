#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Checks a release tree against the inventory of the third-party code it ships (third-party.txt):
# every shared library and executable must match a line, every line must name a license, and
# each license a matching line names must be a file, or a directory holding one. A file missing
# from the inventory fails the check, so a new dependency ships only once its license is
# reviewed and added. Files under a licenses/ directory are the licenses themselves.
#
# Usage: check-licenses.sh <prefix> <inventory>
set -eu
export LC_ALL=C

here=$(cd "$(dirname "$0")" && pwd)
prefix=$1
inventory=$2

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ -d "$prefix" ] || fail "no release tree at $prefix"
[ -f "$inventory" ] || fail "no inventory at $inventory"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

(cd "$prefix" && find . -path '*/licenses' -prune -o -type f -print) |
    sed 's|^\./||' | sort > "$work/files"
(cd "$prefix" && find . -path '*/licenses' -prune -o -type f \
    \( -name '*.so' -o -name '*.so.*' -o -perm -100 \) -print) |
    sed 's|^\./||' | sort > "$work/shipped"
awk -f "$here/match-inventory.awk" "$inventory" "$work/files" > "$work/matched"

awk -F '\t' '$1 == "file" && $2 == 0 { print $3 }' "$work/matched" | sort > "$work/unmatched"
comm -12 "$work/unmatched" "$work/shipped" | sed 's/^/not in the inventory: /' > "$work/problems"
awk -F '\t' '$1 == "entry" && NF < 4 { print "names no license: " $3 }' "$work/matched" >> "$work/problems"

# The licenses named by the lines that match a file.
awk -F '\t' '
    $1 == "entry" { line[$2] = $0; next }
    $1 == "file" && $2 != 0 { used[$2] = 1 }
    END {
        for (n in used) {
            count = split(line[n], field, "\t")
            for (i = 4; i <= count; i++)
                print field[i]
        }
    }' "$work/matched" | sort -u > "$work/licenses"
while IFS= read -r license; do
    if [ -f "$prefix/$license" ] && [ -s "$prefix/$license" ]; then
        continue
    fi
    if [ -d "$prefix/$license" ] && [ -n "$(find "$prefix/$license" -type f -size +0 | head -n 1)" ]; then
        continue
    fi
    echo "no license at $license" >> "$work/problems"
done < "$work/licenses"

if [ -s "$work/problems" ]; then
    cat "$work/problems" >&2
    fail "$prefix does not match $inventory"
fi
echo "PASS: the $(wc -l < "$work/shipped" | tr -d ' ') libraries and executables of $prefix are in the inventory, with their licenses"
