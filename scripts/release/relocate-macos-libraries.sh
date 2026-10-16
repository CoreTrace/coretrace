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
# Then every library that a Mach-O file of the tree loads, the bundled tools' included, must be
# the system's (/usr/lib, /System) or be found in the tree as dyld finds it: @loader_path from the
# file's directory, @executable_path from the directory of the executable that loads it, @rpath
# through the file's own run paths that point into the tree. Any other library fails and is
# named: ctrace would not start, or the tool would not run, once the tree is copied elsewhere.
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

root=$(cd "$prefix" && pwd -P)

# Whether path $1, relative to the prefix and possibly holding .., names a file of the tree.
in_tree() {
    directory=$(cd "$prefix/$(dirname "$1")" 2> /dev/null && pwd -P) || return 1
    case $directory/ in
        "$root"/*) [ -e "$directory/${1##*/}" ] ;;
        *) return 1 ;;
    esac
}

# The directory, relative to the prefix, of the executable that loads file $1: the file's own
# for an executable, else the nearest directory above the file that holds an executable.
executable_directory() {
    directory=$(dirname "$1")
    case $(file -b "$prefix/$1") in
        *executable*)
            echo "$directory"
            return 0
            ;;
    esac
    while :; do
        for candidate in "$prefix/$directory"/*; do
            [ -f "$candidate" ] && [ -x "$candidate" ] || continue
            case $(file -b "$candidate") in
                Mach-O*executable*)
                    echo "$directory"
                    return 0
                    ;;
            esac
        done
        [ "$directory" != . ] || return 1
        directory=$(dirname "$directory")
    done
}

# What path $2, starting with @loader_path or @executable_path, names for file $1, relative to
# the prefix; nothing for any other path, an absolute run path for instance.
expand() {
    case $2 in
        @loader_path | @loader_path/*)
            echo "$(dirname "$1")${2#@loader_path}"
            ;;
        @executable_path | @executable_path/*)
            directory=$(executable_directory "$1") || return 0
            echo "$directory${2#@executable_path}"
            ;;
    esac
}

# Whether reference $2 of file $1 loads a library of the tree, as dyld resolves it.
loads_from_tree() {
    case $2 in
        @rpath/*)
            otool -l "$prefix/$1" |
                awk '$1 == "cmd" { run_path = ($2 == "LC_RPATH") } run_path && $1 == "path" { print $2 }' \
                    > "$work/run-paths"
            while IFS= read -r run_path; do
                base=$(expand "$1" "$run_path")
                if [ -n "$base" ] && in_tree "$base/${2#@rpath/}"; then
                    return 0
                fi
            done < "$work/run-paths"
            return 1
            ;;
        @loader_path/* | @executable_path/*)
            target=$(expand "$1" "$2")
            [ -n "$target" ] && in_tree "$target"
            ;;
        *) return 1 ;;
    esac
}

: > "$work/problems"
macho_files . > "$work/checked"
while IFS= read -r file; do
    dependencies "$prefix/$file" > "$work/dependencies"
    while IFS= read -r dependency; do
        case $dependency in
            /usr/lib/* | /System/*) ;;
            @*)
                loads_from_tree "$file" "$dependency" ||
                    echo "$file loads $dependency, which is not in the tree" >> "$work/problems"
                ;;
            *) echo "$file refers to $dependency, outside the tree" >> "$work/problems" ;;
        esac
    done < "$work/dependencies"
done < "$work/checked"

if [ -s "$work/problems" ]; then
    cat "$work/problems" >&2
    fail "$prefix is not relocatable"
fi
echo "PASS: the $(wc -l < "$work/checked" | tr -d ' ') Mach-O files of $prefix load only the system's libraries and their own"
