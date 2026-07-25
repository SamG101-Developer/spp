#!/usr/bin/env bash
# Publish the dependency prefixes so the configure step finds them without any per-workflow wiring.
set -euo pipefail

if [ "$RUNNER_OS" = "Windows" ]; then
  echo "CMAKE_PREFIX_PATH=$(cygpath -m "$SPP_LOCAL_PREFIX");$(cygpath -m "$SPP_LLVM_WIN_PREFIX");${BOOST_ROOT}" >> "$GITHUB_ENV"
else
  llvm_prefix="${LLVM_PREFIX:-/usr/lib/llvm-${LLVM_LIB_VERSION}}"
  echo "CMAKE_PREFIX_PATH=$SPP_LOCAL_PREFIX:${llvm_prefix}:${BOOST_ROOT}${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}" >> "$GITHUB_ENV"
fi
