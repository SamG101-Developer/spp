#!/usr/bin/env bash
# Publish the dependency prefixes so the configure step
# finds them without any per-workflow wiring.
set -euo pipefail
source .github/scripts/lib/llvm-prefix.sh

llvm_dir="$(llvm_prefix)"
if [ "$RUNNER_OS" = "Windows" ]; then
  # The zlib prefix is Windows-only: it exists to satisfy the
  # ZLIB::ZLIB dependency the prebuilt LLVM exports, which the
  # other two platforms already have a system zlib for.
  zlib_dir="$(cygpath -m "$SPP_ZLIB_WIN_PREFIX")"
  echo "CMAKE_PREFIX_PATH=$(cygpath -m "$SPP_LOCAL_PREFIX");${llvm_dir};${BOOST_ROOT};${zlib_dir}" >> "$GITHUB_ENV"
else
  echo "CMAKE_PREFIX_PATH=$SPP_LOCAL_PREFIX:${llvm_dir}:${BOOST_ROOT}${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}" >> "$GITHUB_ENV"
fi
