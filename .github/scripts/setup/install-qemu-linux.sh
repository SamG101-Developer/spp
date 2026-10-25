#!/usr/bin/env bash
# Unpack the pinned Debian qemu-user (static) into SPP_QEMU_PREFIX and put it on PATH.
set -euo pipefail

source .github/scripts/lib/verified-fetch.sh
# The pin is "qemu-user", not "qemu": qemu reads QEMU_VERSION from the environment as -version.
url="https://deb.debian.org/debian/pool/main/q/qemu/qemu-user_${QEMU_USER_VERSION}_amd64.deb"
deb="${RUNNER_TEMP}/qemu-user.deb"

verified_fetch "$url" "$deb" "$QEMU_USER_SHA256"
rm -rf "$SPP_QEMU_PREFIX"
mkdir -p "$SPP_QEMU_PREFIX"
dpkg-deb -x "$deb" "$SPP_QEMU_PREFIX"
echo "${SPP_QEMU_PREFIX}/usr/bin" >> "$GITHUB_PATH"
"${SPP_QEMU_PREFIX}/usr/bin/qemu-x86_64" --version | head -1
