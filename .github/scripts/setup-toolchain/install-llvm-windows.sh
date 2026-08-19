#!/usr/bin/env bash
# Windows gets the official clang+llvm release tarball,
# which ships the static libs and lib/cmake/llvm that
# "find_package(LLVM)" needs.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

url="https://github.com/llvm/llvm-project/releases/download/${LLVM_WIN_TAG}/${LLVM_WIN_ASSET//+/%2B}"
tmp="$(cygpath -u "$RUNNER_TEMP")"
prefix="$(cygpath -u "$SPP_LLVM_WIN_PREFIX")"
verified_fetch "$url" "$tmp/llvm.tar.xz" "$LLVM_WIN_SHA256"
mkdir -p "$prefix"
tar xf "$tmp/llvm.tar.xz" -C "$prefix" --strip-components=1
