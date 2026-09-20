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

keyring=/etc/apt/keyrings/apt.llvm.org.gpg
install -d -m 0755 /etc/apt/keyrings
# --retry alone ignores connection failures, which is how this step flakes in CI.
curl -fsSL --retry 5 --retry-delay 2 --retry-all-errors --connect-timeout 15 \
    https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o "${keyring}"
echo "deb [signed-by=${keyring}] http://apt.llvm.org/${codename}/ llvm-toolchain-${codename}-${llvm_version} main" \
    > /etc/apt/sources.list.d/apt.llvm.org.list

apt-get update
apt-get install -y --no-install-recommends -o Acquire::Retries=3 \
    "llvm-${llvm_version}" \
    "llvm-${llvm_version}-dev" \
    "clang-${llvm_version}" \
    "libclang-${llvm_version}-dev"
