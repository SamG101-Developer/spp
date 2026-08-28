#!/usr/bin/env bash
# Unpack the prebuilt Boost for this runner into $HOME/boost
# and export BOOST_ROOT. Building Boost from source costs
# far more than the download. The directory is not one of
# the SPP_*_PREFIX paths because it is not ours to pick: the
# tarball carries a top-level boost/ and is unpacked into $HOME.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

base="https://github.com/MarkusJx/prebuilt-boost/releases/download/${BOOST_VERSION}"

# Keyed on the image rather than on RUNNER_OS/RUNNER_ARCH,
# because those cannot tell 22.04 from 24.04 or a 2022 image
# from a 2025 one, and the tarballs are not interchangeable
# across either: each is built against that image's libstdc++
# or msvc runtime.
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
    # Never guess: an unpinned asset is an unverified download,
    # and the digest check below is the only thing standing
    # between CI and whatever the CDN decides to serve.
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
