#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Installs LLVM/Clang <version> from apt.llvm.org on Ubuntu without llvm.sh.
# llvm.sh probes https://apt.llvm.org/<codename>/ with a HEAD request and aborts with
# "Distribution not supported" whenever that probe fails, which breaks CI at random.
# Configuring the repository directly is deterministic. Must run as root.
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <llvm-major-version>" >&2
    exit 2
fi
llvm_version="$1"

# shellcheck disable=SC1091
codename="$(. /etc/os-release && echo "${VERSION_CODENAME}")"
if [[ -z "${codename}" ]]; then
    echo "cannot determine the Ubuntu codename from /etc/os-release" >&2
    exit 1
fi

# apt.llvm.org drops connections and DNS for up to a minute or so, which failed releases at
# every step below: each network step is retried, 5 times over about 2.5 minutes.
# shellcheck source=retry.sh
. "$(dirname "$0")/retry.sh"

keyring=/etc/apt/keyrings/apt.llvm.org.gpg
install -d -m 0755 /etc/apt/keyrings
fetch_key() {
    curl -fsSL --connect-timeout 15 https://apt.llvm.org/llvm-snapshot.gpg.key |
        gpg --batch --yes --dearmor -o "${keyring}"
}
retry 5 10 fetch_key
echo "deb [signed-by=${keyring}] http://apt.llvm.org/${codename}/ llvm-toolchain-${codename}-${llvm_version} main" \
    > /etc/apt/sources.list.d/apt.llvm.org.list

# apt-get update only warns when it cannot fetch an index, and exits 0: the failure shows up at
# install, as packages it cannot locate. Each attempt therefore refreshes the index first.
install_llvm() {
    apt-get update &&
        apt-get install -y --no-install-recommends \
            "llvm-${llvm_version}" \
            "llvm-${llvm_version}-dev" \
            "clang-${llvm_version}" \
            "libclang-${llvm_version}-dev"
}
retry 5 10 install_llvm
