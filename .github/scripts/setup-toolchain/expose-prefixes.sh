#!/usr/bin/env bash
# Publish the dependency prefixes so the configure step
# finds them without any per-workflow wiring.
set -euo pipefail
source .github/scripts/lib/llvm-prefix.sh

llvm_dir="$(llvm_prefix)"
if [ "$RUNNER_OS" = "Windows" ]; then
  # The last prefix is Windows-only: zlib, zstd and libxml2 for
  # the targets the prebuilt LLVM exports, which the other two
  # platforms already have system copies of.
  deps_dir="$(cygpath -m "$SPP_LLVM_DEPS_WIN_PREFIX")"
  echo "CMAKE_PREFIX_PATH=$(cygpath -m "$SPP_LOCAL_PREFIX");${llvm_dir};${BOOST_ROOT};${deps_dir}" >> "$GITHUB_ENV"
else
  echo "CMAKE_PREFIX_PATH=$SPP_LOCAL_PREFIX:${llvm_dir}:${BOOST_ROOT}${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}" >> "$GITHUB_ENV"
fi
