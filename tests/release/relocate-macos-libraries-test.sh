#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# relocate-macos-libraries.sh on a small installation laid out as CMake installs ctrace on macOS:
# an executable and two libraries copied into lib/, each still referring to the others by the
# absolute paths they were built at, as Homebrew's do. Once relocated, the tree runs from another
# directory with the build directory gone, its code signatures are valid, and a reference left
# outside the tree fails the check and is named.
set -u

here=$(cd "$(dirname "$0")" && pwd)
relocate=$here/../../scripts/release/relocate-macos-libraries.sh
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0

# <expected exit status> <description> <text the output must contain, or nothing> <prefix>
run() {
    output=$(sh "$relocate" "$4" 2>&1)
    status=$?
    if [ "$status" -ne "$1" ] || { [ -n "$3" ] && ! printf '%s\n' "$output" | grep -qF -- "$3"; }; then
        echo "[FAIL] $2 (exit code $status)"
        printf '%s\n' "$output" | sed 's/^/    /'
        failures=$((failures + 1))
    else
        echo "[PASS] $2"
    fi
}

expect() {
    if eval "$1"; then
        echo "[PASS] $2"
    else
        echo "[FAIL] $2"
        failures=$((failures + 1))
    fi
}

# libbar, then libfoo that calls it, then app that calls libfoo, each linked by absolute path.
built=$work/built
mkdir -p "$built"
printf 'int bar(void) { return 40; }\n' > "$built/bar.c"
printf 'int bar(void);\nint foo(void) { return bar() + 2; }\n' > "$built/foo.c"
printf '#include <stdio.h>\nint foo(void);\nint main(void) { printf("%%d\\n", foo()); return 0; }\n' \
    > "$built/app.c"
cc -dynamiclib -o "$built/libbar.1.dylib" -install_name "$built/libbar.1.dylib" "$built/bar.c"
cc -dynamiclib -o "$built/libfoo.dylib" -install_name "$built/libfoo.dylib" "$built/foo.c" \
    "$built/libbar.1.dylib"
# ctrace gets this run path from CMake (INSTALL_RPATH).
cc -o "$built/app" "$built/app.c" "$built/libfoo.dylib" -Wl,-rpath,@loader_path/../lib

install_tree() {
    rm -rf "$1"
    mkdir -p "$1/bin" "$1/lib"
    cp "$built/app" "$1/bin/app"
    cp "$built/libfoo.dylib" "$built/libbar.1.dylib" "$1/lib/"
    ln -s libbar.1.dylib "$1/lib/libbar.dylib"
    # Homebrew's libraries are read-only.
    chmod 0444 "$1/lib/libfoo.dylib" "$1/lib/libbar.1.dylib"
}

prefix=$work/prefix
install_tree "$prefix"
run 0 "an installation that refers to its build directory is relocated" "" "$prefix"
expect '! otool -L "$prefix/bin/app" "$prefix/lib/libfoo.dylib" "$prefix/lib/libbar.1.dylib" | grep -qF "$built"' \
    "no reference to the build directory is left"
expect 'otool -D "$prefix/lib/libbar.1.dylib" | grep -qx "@rpath/libbar.1.dylib"' \
    "each library is identified through the run path"
expect 'codesign --verify "$prefix/bin/app" && codesign --verify "$prefix/lib/libfoo.dylib"' \
    "the modified files are signed again"

moved=$work/elsewhere/coretrace
mkdir -p "$work/elsewhere"
mv "$prefix" "$moved"
mv "$built" "$work/gone"
expect '[ "$("$moved/bin/app")" = 42 ]' "the relocated tree runs from another directory, without its build directory"
mv "$work/gone" "$built"

run 0 "relocating an installation twice changes nothing" "" "$moved"
mv "$built" "$work/gone"
expect '[ "$("$moved/bin/app")" = 42 ]' "the tree relocated twice still runs"
mv "$work/gone" "$built"

install_tree "$prefix"
rm "$prefix/lib/libbar.1.dylib" "$prefix/lib/libbar.dylib"
run 1 "a library that is not in the tree fails and is named" \
    "lib/libfoo.dylib refers to $built/libbar.1.dylib, outside the tree" "$prefix"

if [ "$failures" -ne 0 ]; then
    echo "$failures relocate-macos-libraries test(s) failed"
    exit 1
fi
echo "relocate-macos-libraries tests: all passed"
