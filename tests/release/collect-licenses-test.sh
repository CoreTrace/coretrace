#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
#
# collect-licenses.sh against stand-ins for dpkg, dpkg-query and rpm: each package the inventory
# names gets its license and exact version, and a library the package does not own, or a package
# without a license file, fails and is named.
set -u

here=$(cd "$(dirname "$0")" && pwd)
collect=$here/../../scripts/release/collect-licenses.sh
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0

# <expected exit status> <description> <text the output must contain, or nothing> <arguments...>
run() {
    expected=$1 description=$2 needle=$3
    shift 3
    output=$(PATH="$work/bin:$PATH" sh "$collect" "$@" 2>&1)
    status=$?
    if [ "$status" -ne "$expected" ] || { [ -n "$needle" ] && ! printf '%s\n' "$output" | grep -qF -- "$needle"; }; then
        echo "[FAIL] $description (exit code $status)"
        printf '%s\n' "$output" | sed 's/^/    /'
        failures=$((failures + 1))
    else
        echo "[PASS] $description"
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

# Stand-ins that answer from the files below.
mkdir -p "$work/bin"
cat > "$work/bin/dpkg" <<'STUB'
#!/bin/sh
[ "$1" = -S ] || exit 2
awk -v name="${2##*/}" '$1 == name { printf "%s:amd64: /usr/lib/x86_64-linux-gnu/%s\n", $2, $1; found = 1 }
    END { exit !found }' "$FAKE_DPKG_OWNERS"
STUB
cat > "$work/bin/dpkg-query" <<'STUB'
#!/bin/sh
for package; do :; done
echo "$package 1.2-3ubuntu1"
STUB
cat > "$work/bin/rpm" <<'STUB'
#!/bin/sh
case "$1" in
    -qa) cat "$FAKE_RPM_FILES" ;;
    -qL) awk -v p="$2" '$1 == p { print $2 }' "$FAKE_RPM_LICENSES" ;;
    -q) for package; do :; done; echo "$package 4.5-6.el9" ;;
    *) exit 2 ;;
esac
STUB
chmod +x "$work/bin/dpkg" "$work/bin/dpkg-query" "$work/bin/rpm"

# The Ubuntu side: ctrace's libraries, the same library in a bundled tool, Clang's headers.
tree=$work/ubuntu
mkdir -p "$tree/lib/clang/20/include" "$tree/libexec/tool/lib" "$tree/bin"
printf 'x' > "$tree/lib/libfoo.so.1.2"
ln -s libfoo.so.1.2 "$tree/lib/libfoo.so.1"
cp "$tree/lib/libfoo.so.1.2" "$tree/libexec/tool/lib/libfoo.so.1.2"
printf 'x' > "$tree/lib/clang/20/include/stddef.h"
mkdir -p "$work/doc/libfoo1" "$work/doc/libclang-common-20-dev" "$work/common-licenses"
printf 'libfoo copyright\n' > "$work/doc/libfoo1/copyright"
printf 'clang headers copyright\n' > "$work/doc/libclang-common-20-dev/copyright"
printf 'Apache License text\n' > "$work/common-licenses/Apache-2.0"
printf 'libfoo.so.1.2 libfoo1\n' > "$work/dpkg-owners"
cat > "$work/inventory" <<'INVENTORY'
# Package lines, a composite line and a line that matches nothing here.
*lib/libfoo.so.*          licenses/ubuntu/libfoo1
lib/clang/*/include/*     licenses/ubuntu/libclang-common-20-dev
bin/app                   LICENSE licenses/ubuntu/libfoo1
lib/libunused.so.*        licenses/ubuntu/libunused1
libexec/py/libssl.so.*    libexec/py/licenses/rocky/openssl-libs
libexec/py/libpython3*.so.*  libexec/py/licenses/rocky/python3.12-libs
libexec/py/*.so           libexec/py/licenses/rocky/python3.12-libs
INVENTORY
export FAKE_DPKG_OWNERS="$work/dpkg-owners" CORETRACE_DOC_ROOT="$work/doc" CORETRACE_COMMON_LICENSES="$work/common-licenses"

run 0 "dpkg: each package the inventory names for a shipped file gets its copyright and version" "" \
    dpkg "$tree" "$work/inventory" lib libexec/tool/lib
expect 'grep -q "libfoo copyright" "$tree/licenses/ubuntu/libfoo1/copyright"' "dpkg: the package's copyright file is copied"
expect 'grep -qx "libfoo1 1.2-3ubuntu1" "$tree/licenses/ubuntu/libfoo1/VERSION"' "dpkg: the exact package version is recorded"
expect 'test -s "$tree/licenses/ubuntu/libclang-common-20-dev/copyright"' "dpkg: a directory of headers gets its package's copyright"
expect 'test -s "$tree/licenses/ubuntu/common-licenses/Apache-2.0"' "dpkg: the common licenses the copyright files refer to are copied"
expect '! test -e "$tree/licenses/ubuntu/libunused1"' "dpkg: a package whose files are not shipped is not collected"
expect '! test -e "$tree/licenses/rocky" && ! test -e "$tree/libexec/py"' "dpkg: rpm packages are left to the rpm side"

printf 'libfoo.so.1.2 libother1\n' > "$work/dpkg-owners"
run 1 "dpkg: a library the named package does not own fails, with both packages named" \
    "lib/libfoo.so.1.2 comes from libother1, not libfoo1" dpkg "$tree" "$work/inventory" lib libexec/tool/lib
printf 'libfoo.so.1.2 libfoo1\n' > "$work/dpkg-owners"

rm "$work/doc/libfoo1/copyright"
run 1 "dpkg: a package without a copyright file fails and is named" \
    "libfoo1 has no copyright file" dpkg "$tree" "$work/inventory" lib libexec/tool/lib

# The Rocky Linux side: the Python analyzer's directory, collected where it is built.
rocky=$work/rocky
dist=$rocky/libexec/py
mkdir -p "$dist"
printf 'x' > "$dist/libssl.so.3"
printf 'x' > "$dist/_ssl.so"
printf 'x' > "$dist/libpython3.12.so.1.0"
mkdir -p "$work/rpm-licenses"
printf 'OpenSSL license\n' > "$work/rpm-licenses/LICENSE.txt"
printf 'PSF license\n' > "$work/rpm-licenses/PSF-LICENSE.txt"
# The system's Python 3.9 has the same modules: the one shipped belongs to the Python whose
# library is shipped with it.
{
    printf 'python3-libs\t/usr/lib64/python3.9/lib-dynload/_ssl.cpython-39-x86_64-linux-gnu.so\n'
    printf 'openssl-libs\t/usr/lib64/libssl.so.3\n'
    printf 'python3.12-libs\t/usr/lib64/libpython3.12.so.1.0\n'
    printf 'python3.12-libs\t/usr/lib64/python3.12/lib-dynload/_ssl.cpython-312-x86_64-linux-gnu.so\n'
} > "$work/rpm-files"
printf 'openssl-libs %s\npython3.12-libs %s\n' "$work/rpm-licenses/LICENSE.txt" "$work/rpm-licenses/PSF-LICENSE.txt" > "$work/rpm-license-list"
export FAKE_RPM_FILES="$work/rpm-files" FAKE_RPM_LICENSES="$work/rpm-license-list"

run 0 "rpm: a library and a Python extension module get their packages' licenses" "" \
    rpm "$rocky" "$work/inventory" libexec/py
expect 'grep -q "OpenSSL license" "$dist/licenses/rocky/openssl-libs/LICENSE.txt"' "rpm: the package's license files are copied"
expect 'grep -qx "openssl-libs 4.5-6.el9" "$dist/licenses/rocky/openssl-libs/VERSION"' "rpm: the exact package version is recorded"
expect 'test -s "$dist/licenses/rocky/python3.12-libs/PSF-LICENSE.txt"' "rpm: an extension module maps to the package of its .cpython- file, for the shipped Python"

mkdir -p "$dist/yaml"
printf 'x' > "$dist/yaml/_yaml.so"
run 1 "rpm: a module no package owns fails and is named" \
    "libexec/py/yaml/_yaml.so comes from no package, not python3.12-libs" rpm "$rocky" "$work/inventory" libexec/py

if [ "$failures" -ne 0 ]; then
    echo "$failures collect-licenses test(s) failed"
    exit 1
fi
echo "collect-licenses tests: all passed"
