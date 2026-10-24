#!/usr/bin/env bash
# Unpack the pinned Debian qemu-user (static) into SPP_QEMU_PREFIX and put it on PATH.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
url="https://deb.debian.org/debian/pool/main/q/qemu/qemu-user_${QEMU_VERSION}_amd64.deb"
deb="${RUNNER_TEMP}/qemu-user.deb"

verified_fetch "$url" "$deb" "$QEMU_SHA256"
rm -rf "$SPP_QEMU_PREFIX"
mkdir -p "$SPP_QEMU_PREFIX"
dpkg-deb -x "$deb" "$SPP_QEMU_PREFIX"
echo "${SPP_QEMU_PREFIX}/usr/bin" >> "$GITHUB_PATH"
"${SPP_QEMU_PREFIX}/usr/bin/qemu-x86_64" --version | head -1
