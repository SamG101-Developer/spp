#!/usr/bin/env bash
# Windows gets the official clang+llvm release tarball, which ships the static libs and lib/cmake/llvm that
# find_package(LLVM) needs.
set -euo pipefail

url="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_LIB_FULL_VERSION}/clang+llvm-${LLVM_LIB_FULL_VERSION}-x86_64-pc-windows-msvc.tar.xz"
tmp="$(cygpath -u "$RUNNER_TEMP")"
curl -fL -o "$tmp/llvm.tar.xz" "$url"
mkdir -p "$SPP_LLVM_WIN_PREFIX"
tar xf "$tmp/llvm.tar.xz" -C "$SPP_LLVM_WIN_PREFIX" --strip-components=1
