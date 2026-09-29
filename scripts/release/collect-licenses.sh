#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Copies into a release tree the licenses of the system packages its libraries come from, where
# the inventory (third-party.txt) names them: each license path <...>/licenses/<distribution>/
# <package> of a line that matches a file of the given directories receives the package's license
# files and a VERSION file with its exact version. The package must provide every shared library
# the line matches: a library that comes from another package, or from none, fails the run, as a
# new provider needs its license reviewed. Run it on the machine that built the directories, with
# only the directories built from its packages.
#
#   dpkg: /usr/share/doc/<package>/copyright, and /usr/share/common-licenses, to which copyright
#         files refer for the full texts of common licenses.
#   rpm:  the license files of the package (rpm -qL).
#
# Usage: collect-licenses.sh <dpkg|rpm> <prefix> <inventory> <directory relative to prefix>...
set -eu
export LC_ALL=C

here=$(cd "$(dirname "$0")" && pwd)
database=$1
prefix=$2
inventory=$3
shift 3
doc_root=${CORETRACE_DOC_ROOT:-/usr/share/doc}
common_licenses=${CORETRACE_COMMON_LICENSES:-/usr/share/common-licenses}

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
case $database in
    dpkg | rpm) ;;
    *) fail "unknown package database '$database': dpkg or rpm" ;;
esac
[ "$#" -gt 0 ] || fail "no directory to collect"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
: > "$work/collected"

for directory; do
    [ -d "$prefix/$directory" ] || fail "no directory $prefix/$directory"
    (cd "$prefix" && find "$directory" -path '*/licenses' -prune -o -type f -print)
done | sort > "$work/files"
awk -f "$here/match-inventory.awk" "$inventory" "$work/files" > "$work/matched"

if [ "$database" = rpm ]; then
    rpm -qa --qf '[%{=NAME}\t%{FILENAMES}\n]' > "$work/rpm-files"
    # The extension modules shipped belong to the Python whose library is shipped with them, not
    # to another Python of the machine: libpython3.12.so gives the tag of 3.12's, cpython-312.
    python_tag=$(sed -n 's|.*/libpython\([0-9]*\)\.\([0-9]*\)\.so.*|cpython-\1\2|p' "$work/files" | head -n 1)
fi

# The package that provides the shipped library at archive path $1, or nothing.
provider() {
    name=${1##*/}
    if [ "$database" = dpkg ]; then
        dpkg -S "*/$name" 2>/dev/null | grep -v '^diversion' | head -n 1 |
            sed 's/: .*//; s/,.*//; s/:.*//'
        return 0
    fi
    # Nuitka ships CPython's extension modules as <module>.so, from <module>.<tag>-<platform>.so.
    awk -F '\t' -v name="$name" -v tag="${python_tag:-}" '
        {
            count = split($2, part, "/")
            base = part[count]
            if (name ~ /^lib/) {
                if (base == name) { print $1; exit }
            } else if (tag != "") {
                module = substr(name, 1, length(name) - 3)
                if (index(base, module "." tag "-") == 1 && base ~ /\.so$/) { print $1; exit }
            }
        }' "$work/rpm-files"
}

# Copies the license and the version of package $1 to license path $2, once per run.
collect() {
    if grep -qxF -- "$2" "$work/collected"; then
        return 0
    fi
    echo "$2" >> "$work/collected"
    target=$prefix/$2
    mkdir -p "$target"
    if [ "$database" = dpkg ]; then
        [ -f "$doc_root/$1/copyright" ] || fail "$1 has no copyright file in $doc_root/$1"
        cp "$doc_root/$1/copyright" "$target/copyright"
        dpkg-query -W -f='${Package} ${Version}\n' "$1" > "$target/VERSION"
        common=${target%/*}/common-licenses
        if [ -d "$common_licenses" ] && [ ! -d "$common" ]; then
            mkdir -p "$common"
            cp "$common_licenses"/* "$common/"
        fi
    else
        files=$(rpm -qL "$1")
        case $files in
            */*) ;;
            *) fail "$1 has no license file" ;;
        esac
        printf '%s\n' "$files" | while IFS= read -r file; do
            cp "$file" "$target/"
        done
        rpm -q --qf '%{NAME} %{VERSION}-%{RELEASE}\n' "$1" > "$target/VERSION"
    fi
    echo "collected $2"
}

# The package license paths, <...>licenses/<distribution>/<package>, of the lines that match.
awk -F '\t' '
    $1 == "entry" { for (i = 4; i <= NF; i++) if ($i ~ /(^|\/)licenses\/[^\/]+\/[^\/]+$/) path[$2] = path[$2] "\t" $i; next }
    $1 == "file" && $2 != 0 && ($2 in path) { print $2 path[$2]; delete path[$2] }
' "$work/matched" > "$work/packages"

while IFS= read -r entry; do
    line=${entry%%"$(printf '\t')"*}
    licenses=${entry#*"$(printf '\t')"}
    awk -F '\t' -v line="$line" '$1 == "file" && $2 == line { print $3 }' "$work/matched" > "$work/line-files"
    for license in $(printf '%s\n' "$licenses" | tr '\t' ' '); do
        package=${license##*/}
        while IFS= read -r file; do
            case $file in
                *.so | *.so.*)
                    found=$(provider "$file")
                    [ "$found" = "$package" ] || fail "$file comes from ${found:-no package}, not $package"
                    ;;
            esac
        done < "$work/line-files"
        collect "$package" "$license"
    done
done < "$work/packages"
