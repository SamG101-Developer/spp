#!/usr/bin/env bash
# Unpack the prebuilt Boost for this runner into $HOME/boost and
# export BOOST_ROOT: building it from source costs far more than
# the download. The path is the tarball's own top-level boost/,
# which is why it is not an SPP_*_PREFIX.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

base="https://github.com/MarkusJx/prebuilt-boost/releases/download/${BOOST_VERSION}"

# Keyed on the image, not RUNNER_OS/RUNNER_ARCH: each tarball is
# built against that image's libstdc++ or MSVC runtime, and the
# two cannot tell 22.04 from 24.04.
if [ -z "${SPP_RUNNER_IMAGE:-}" ]; then
  echo "::error::SPP_RUNNER_IMAGE is not set; setup-toolchain must be given its runner-image input"
  exit 1
fi

case "$SPP_RUNNER_IMAGE" in
  ubuntu-24.04)
    url="$base/boost-${BOOST_VERSION}-ubuntu-24.04-gcc-static%2Bshared-x86.tar.gz"
    sha="$BOOST_SHA256_UBUNTU_2404"
    ;;
  ubuntu-24.04-arm)
    url="$base/boost-${BOOST_VERSION}-ubuntu-22.04-gcc-static%2Bshared-aarch64.tar.gz"
    sha="$BOOST_SHA256_UBUNTU_2204_ARM64"
    ;;
  macos-15)
    url="$base/boost-${BOOST_VERSION}-macos-15-clang-static%2Bshared-aarch64.tar.gz"
    sha="$BOOST_SHA256_MACOS_15"
    ;;
  macos-26)
    url="$base/boost-${BOOST_VERSION}-macos-26-clang-static%2Bshared-aarch64.tar.gz"
    sha="$BOOST_SHA256_MACOS_26"
    ;;
  windows-2025)
    url="$base/boost-${BOOST_VERSION}-windows-2025-msvc-static-x86.tar.gz"
    sha="$BOOST_SHA256_WINDOWS_2025"
    ;;
  windows-2022)
    url="$base/boost-${BOOST_VERSION}-windows-2022-msvc-static-x86.tar.gz"
    sha="$BOOST_SHA256_WINDOWS_2022"
    ;;
  *)
    # Never guess: an unknown image is an error. Updates can be
    # requested from the prebuilt-boost repository.
    echo "::error::no Boost asset is pinned for ${SPP_RUNNER_IMAGE}"
    echo "::error::Add one to .github/scripts/setup-toolchain/install-boost.sh, .github/dependencies.toml and refresh-pins.sh."
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
