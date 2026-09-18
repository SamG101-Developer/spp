#!/usr/bin/env bash
# Publish the environment the build and test steps share:
# the compiler version this job's compiler is pinned to,
# the sanitiser options (inert for a build without one),
# and the shard suffix that keeps artefact names apart.
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

suffix=""
if [ "$SHARD_COUNT" -gt 1 ]; then
  suffix="-shard${SHARD_INDEX}"
fi

{
  echo "SPP_TOOLCHAIN_VERSION=${version}"
  echo "SPP_TEST_MODE=${SPP_MODE}"
  echo "SPP_SHARD_SUFFIX=${suffix}"
  echo "ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1:strict_string_checks=1"
  echo "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
  echo "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1"
} >> "$GITHUB_ENV"

echo "${COMPILER} ${version:-(image default)}, s++ mode ${SPP_MODE}, shard ${SHARD_INDEX}/${SHARD_COUNT}"
