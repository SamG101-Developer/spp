#!/usr/bin/env bash
# Unpack the pinned static qemu-user into SPP_QEMU_PREFIX and put it on PATH: a release of our qemu fork, which carries
# the linux-user fixes our tests need before qemu ships them (upstream-watch U17-U19).
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
# The pin is "qemu-user", not "qemu": qemu reads QEMU_VERSION from the environment as -version.
url="https://github.com/SamG101-Developer/qemu/releases/download/${QEMU_USER_TAG}/qemu-user-static-${QEMU_USER_TAG}-x86_64.tar.xz"
tarball="${RUNNER_TEMP}/qemu-user.tar.xz"

verified_fetch "$url" "$tarball" "$QEMU_USER_SHA256"
rm -rf "$SPP_QEMU_PREFIX"
mkdir -p "$SPP_QEMU_PREFIX"
tar -xJf "$tarball" -C "$SPP_QEMU_PREFIX"
echo "${SPP_QEMU_PREFIX}/bin" >> "$GITHUB_PATH"
"${SPP_QEMU_PREFIX}/bin/qemu-x86_64" --version | head -1
