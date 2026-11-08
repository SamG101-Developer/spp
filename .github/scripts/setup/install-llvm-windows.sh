#!/usr/bin/env bash
# Windows gets the official clang+llvm release tarball,
# which ships the static libs and lib/cmake/llvm that
# "find_package(LLVM)" needs.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

case "$RUNNER_ARCH" in
  X64) tag="$LLVM_WIN_TAG" asset="$LLVM_WIN_ASSET" sha="$LLVM_WIN_SHA256" ;;
  ARM64) tag="$LLVM_WIN_ARM_TAG" asset="$LLVM_WIN_ARM_ASSET" sha="$LLVM_WIN_ARM_SHA256" ;;
  *)
    echo "::error::no LLVM release is pinned for Windows-${RUNNER_ARCH}"
    exit 1
    ;;
esac

url="https://github.com/llvm/llvm-project/releases/download/${tag}/${asset//+/%2B}"
tmp="$(cygpath -u "$RUNNER_TEMP")"
prefix="$(cygpath -u "$SPP_CI_LLVM_WIN_PREFIX")"
verified_fetch "$url" "$tmp/llvm.tar.xz" "$sha"
mkdir -p "$prefix"
tar xf "$tmp/llvm.tar.xz" -C "$prefix" --strip-components=1
