#!/usr/bin/env bash
# Put the pinned compiler cache on PATH, checksum first:
# sccache on Windows, ccache elsewhere. Distro ccache is too
# old: before 4.14.1 it does not hash -fmodule-file= BMIs, and
# serves clang importers stale objects.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

if [ "$RUNNER_OS" = "Windows" ]; then
  asset="sccache-v${SCCACHE_VERSION}-x86_64-pc-windows-msvc"
  verified_install_bin \
    "https://github.com/mozilla/sccache/releases/download/v${SCCACHE_VERSION}/${asset}.tar.gz" \
    "$SCCACHE_SHA256" sccache.exe "${asset}/sccache.exe"
  exit 0
fi

case "${RUNNER_OS}-${RUNNER_ARCH}" in
  Linux-X64) platform=linux-x86_64-musl-static want="$CCACHE_SHA256_LINUX_X86_64" ;;
  Linux-ARM64) platform=linux-aarch64-musl-static want="$CCACHE_SHA256_LINUX_AARCH64" ;;
  macOS-*) platform=darwin want="$CCACHE_SHA256_DARWIN" ;;
  *)
    echo "::error::no ccache release is pinned for ${RUNNER_OS}-${RUNNER_ARCH}"
    exit 1
    ;;
esac

asset="ccache-${CCACHE_VERSION}-${platform}"
verified_install_bin \
  "https://github.com/ccache/ccache/releases/download/v${CCACHE_VERSION}/${asset}.tar.gz" \
  "$want" ccache "${asset}/ccache"
