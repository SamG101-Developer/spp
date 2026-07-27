#!/usr/bin/env bash
# Unpack the prebuilt Boost for this runner into $HOME/boost and export
# BOOST_ROOT. Building Boost from source costs far more than the download.
# The directory is not one of the SPP_*_PREFIX paths because it is not ours to
# pick: the tarball carries a top-level boost/ and is unpacked into $HOME.
set -euo pipefail

base="https://github.com/MarkusJx/prebuilt-boost/releases/download/${BOOST_VERSION}"
if [ "$RUNNER_OS" = "Windows" ]; then
  url="$base/boost-${BOOST_VERSION}-windows-2025-msvc-static-x86.tar.gz"
elif [ "$RUNNER_OS" = "macOS" ]; then
  url="$base/boost-${BOOST_VERSION}-macos-15-clang-static%2Bshared-aarch64.tar.gz"
else
  url="$base/boost-${BOOST_VERSION}-ubuntu-24.04-gcc-static%2Bshared-x86.tar.gz"
fi

tmp="$RUNNER_TEMP"
[ "$RUNNER_OS" = "Windows" ] && tmp="$(cygpath -u "$tmp")"
curl -fL -o "$tmp/boost.tar.gz" "$url"
tar xf "$tmp/boost.tar.gz" -C "$HOME"   # extracts to $HOME/boost

if [ "$RUNNER_OS" = "Windows" ]; then
  echo "BOOST_ROOT=$(cygpath -m "$HOME/boost")" >> "$GITHUB_ENV"
else
  echo "BOOST_ROOT=$HOME/boost" >> "$GITHUB_ENV"
fi
