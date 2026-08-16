#!/usr/bin/env bash
# Unpack the prebuilt Boost for this runner into $HOME/boost
# and export BOOST_ROOT. Building Boost from source costs
# far more than the download. The directory is not one of
# the SPP_*_PREFIX paths because it is not ours to pick: the
# tarball carries a top-level boost/ and is unpacked into $HOME.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

base="https://github.com/MarkusJx/prebuilt-boost/releases/download/${BOOST_VERSION}"
case "${RUNNER_OS}/${RUNNER_ARCH}" in
  Windows/X64)
    url="$base/boost-${BOOST_VERSION}-windows-2025-msvc-static-x86.tar.gz"
    sha="$BOOST_SHA256_WINDOWS"
    ;;
  macOS/ARM64)
    url="$base/boost-${BOOST_VERSION}-macos-15-clang-static%2Bshared-aarch64.tar.gz"
    sha="$BOOST_SHA256_MACOS"
    ;;
  Linux/X64)
    url="$base/boost-${BOOST_VERSION}-ubuntu-24.04-gcc-static%2Bshared-x86.tar.gz"
    sha="$BOOST_SHA256_LINUX"
    ;;
  Linux/ARM64)
    url="$base/boost-${BOOST_VERSION}-ubuntu-22.04-gcc-static%2Bshared-aarch64.tar.gz"
    sha="$BOOST_SHA256_LINUX_ARM64"
    ;;
  *)
    # Never guess: an unpinned asset is an unverified download,
    # and the digest check below is the only thing standing
    # between CI and whatever the CDN decides to serve.
    echo "::error::no Boost asset is pinned for ${RUNNER_OS}/${RUNNER_ARCH}"
    echo "::error::Add one to .github/scripts/setup-toolchain/install-boost.sh, .github/versions.env and refresh-pins.sh."
    exit 1
    ;;
esac

tmp="$RUNNER_TEMP"
[ "$RUNNER_OS" = "Windows" ] && tmp="$(cygpath -u "$tmp")"
verified_fetch "$url" "$tmp/boost.tar.gz" "$sha"
tar xf "$tmp/boost.tar.gz" -C "$HOME"

if [ "$RUNNER_OS" = "Windows" ]; then
  echo "BOOST_ROOT=$(cygpath -m "$HOME/boost")" >> "$GITHUB_ENV"
else
  echo "BOOST_ROOT=$HOME/boost" >> "$GITHUB_ENV"
fi
