#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# Packages a macOS build of ctrace as the release disk image, as Dockerfile.release-binaries
# packages the Linux archive: the installation, relocated so that it runs from wherever it is
# copied, with the licenses of the third-party code it ships, checked against the macOS inventory
# (third-party-macos.txt). The image holds one folder, coretrace-<version>-macos-<arch>, laid out
# as the Linux archive; a .sha256 file is written next to it.
#
# Run it on the machine that built <build dir>: the licenses come from its Homebrew kegs. The
# files are signed ad hoc, not with a Developer ID, so Gatekeeper blocks them once a browser has
# marked them as downloaded (README, "macOS").
#
# Usage: package-macos.sh <build dir> <version> <output dir>
set -eu

build=$1
version=$2
out=$3
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)

fail() {
    echo "FAIL: $1" >&2
    exit 1
}
[ -f "$build/CMakeCache.txt" ] || fail "no CMake build in $build"
case $(uname -m) in
    arm64) arch=arm64 ;;
    x86_64) arch=amd64 ;;
    *) fail "unsupported architecture: $(uname -m)" ;;
esac

name=coretrace-$version-macos-$arch
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
staging=$work/image/$name

cmake --install "$build" --prefix "$staging" --strip > /dev/null
sh "$here/relocate-macos-libraries.sh" "$staging"

# The image runs on the oldest macOS that all its Mach-O files support. When the build sets a
# deployment target, a file that needs a newer macOS, a Homebrew library built for the runner's
# newer system for instance, fails and is named.
target=$(sed -n 's/^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]*=//p' "$build/CMakeCache.txt" | head -n 1)
if [ -n "$target" ]; then
    (cd "$staging" && find . -type f \( -name '*.dylib' -o -name '*.so' -o -perm -100 \) -print) |
        while IFS= read -r file; do
            minimum=$(otool -l "$staging/$file" 2>/dev/null |
                awk '$1 == "cmd" && $2 == "LC_BUILD_VERSION" { found = 1 } found && $1 == "minos" { print $2; exit }')
            [ -n "$minimum" ] || continue
            if awk -v file="$minimum" -v target="$target" 'BEGIN {
                split(file, f, "."); split(target, t, ".")
                exit !(f[1] + 0 > t[1] + 0 || (f[1] + 0 == t[1] + 0 && f[2] + 0 > t[2] + 0))
            }'; then
                echo "${file#./} needs macOS $minimum, newer than $target"
            fi
        done > "$work/too-new"
    if [ -s "$work/too-new" ]; then
        cat "$work/too-new" >&2
        fail "the image does not run on macOS $target"
    fi
fi
install -m 0644 "$repo/README.md" "$repo/LICENSE" "$repo/NOTICE" "$staging/"

set -- lib
if [ -d "$staging/libexec/coretrace/coretrace-python-analyzer" ]; then
    set -- "$@" libexec/coretrace/coretrace-python-analyzer
fi
sh "$here/collect-licenses.sh" brew "$staging" "$here/third-party-macos.txt" "$@"
sh "$here/check-licenses.sh" "$staging" "$here/third-party-macos.txt"

mkdir -p "$out"
hdiutil create -quiet -volname "$name" -srcfolder "$work/image" -format UDZO -ov "$out/$name.dmg"
(cd "$out" && shasum -a 256 "$name.dmg" > "$name.dmg.sha256")
echo "packaged $out/$name.dmg"
