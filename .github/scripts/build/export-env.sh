#!/usr/bin/env bash
# Publish the compiler version this job's compiler is pinned
# to, which the toolchain and cache-key steps read.
set -euo pipefail

# The version is never named by a workflow: the manifest
# is the one place a toolchain is pinned, so a bump moves
# every job and every cache key with it. MSVC and clang-cl
# ship with the image and have no version to ask for.
case "$COMPILER" in
  gcc) version="$GCC_VERSION" ;;
  clang) version="$LLVM_LIB_VERSION" ;;
  *) version="" ;;
esac

echo "SPP_TOOLCHAIN_VERSION=${version}" >> "$GITHUB_ENV"
echo "${COMPILER} ${version:-(image default)}"
