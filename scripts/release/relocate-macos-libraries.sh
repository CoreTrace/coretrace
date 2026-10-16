#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Makes a macOS installation relocatable, as patchelf does for the Linux archive. CMake copies into
# <prefix>/lib the libraries ctrace links that are not the system's (Homebrew's LLVM, Clang and
# what they load), but each Mach-O file still refers to them by the absolute paths they were built
# at. Here every reference to a library of lib/ becomes @rpath/<name>, each library of lib/ is
# identified as @rpath/<name> and finds the others through @loader_path, and ctrace finds lib/
# through the run path CMake gives it (@loader_path/../lib). Each modified file is signed again,
# ad hoc: Apple silicon kills a program whose signature no longer matches its bytes.
#
# Then every Mach-O file of the tree, the bundled tools' included, may only refer to the system
# (/usr/lib, /System), to lib/ through @rpath, or to paths relative to itself: any other
# reference fails and is named, as ctrace would not start, or the tool would not run, elsewhere.
#
# Usage: relocate-macos-libraries.sh <prefix>
set -eu

prefix=$1

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ -d "$prefix/lib" ] || fail "no directory $prefix/lib"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The Mach-O files under the given directories of the prefix, relative to it. Headers and
# configuration files are left out before asking `file`.
macho_files() {
    (cd "$prefix" && find "$@" -type f \( -name '*.dylib' -o -name '*.so' -o -perm -100 \) -print) |
        while IFS= read -r file; do
            case $(file -b "$prefix/$file") in
                Mach-O*) echo "${file#./}" ;;
            esac
        done
}

# The identifier of library $1, empty for an executable.
identifier() {
    otool -D "$1" | sed -n 2p
}

# The libraries that file $1 loads, without its own identifier.
dependencies() {
    own=$(identifier "$1")
    otool -L "$1" | sed -n '2,$s/^[[:space:]]*\(.*\) (compatibility version.*/\1/p' |
        grep -vxF -- "${own:-/}" || true
}

macho_files bin lib > "$work/relocated"
while IFS= read -r file; do
    path=$prefix/$file
    set --
    case $file in
        lib/*)
            id=$(identifier "$path")
            [ "$id" = "@rpath/${id##*/}" ] || set -- "$@" -id "@rpath/${id##*/}"
            otool -l "$path" | grep -q '^ *path @loader_path (offset' ||
                set -- "$@" -add_rpath @loader_path
            ;;
    esac
    dependencies "$path" > "$work/dependencies"
    while IFS= read -r dependency; do
        case $dependency in
            /usr/lib/* | /System/* | @*) continue ;;
        esac
        name=${dependency##*/}
        if [ -e "$prefix/lib/$name" ]; then
            set -- "$@" -change "$dependency" "@rpath/$name"
        fi
    done < "$work/dependencies"
    [ "$#" -gt 0 ] || continue
    chmod u+w "$path"
    install_name_tool "$@" "$path" 2> "$work/error" ||
        { cat "$work/error" >&2; fail "cannot relocate $file"; }
    codesign --force --sign - "$path" 2> "$work/error" ||
        { cat "$work/error" >&2; fail "cannot sign $file again"; }
    echo "relocated $file"
done < "$work/relocated"

: > "$work/problems"
macho_files . > "$work/checked"
while IFS= read -r file; do
    dependencies "$prefix/$file" > "$work/dependencies"
    while IFS= read -r dependency; do
        case $file:$dependency in
            *:/usr/lib/* | *:/System/*) ;;
            bin/*:@rpath/* | lib/*:@rpath/*)
                [ -e "$prefix/lib/${dependency#@rpath/}" ] ||
                    echo "$file refers to $dependency, which is not in lib/" >> "$work/problems"
                ;;
            *:@rpath/* | *:@loader_path/* | *:@executable_path/*) ;;
            *) echo "$file refers to $dependency, outside the tree" >> "$work/problems" ;;
        esac
    done < "$work/dependencies"
done < "$work/checked"

if [ -s "$work/problems" ]; then
    cat "$work/problems" >&2
    fail "$prefix is not relocatable"
fi
echo "PASS: the $(wc -l < "$work/checked" | tr -d ' ') Mach-O files of $prefix load only the system's libraries and their own"
