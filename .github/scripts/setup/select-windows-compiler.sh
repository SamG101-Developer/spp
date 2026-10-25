#!/usr/bin/env bash
# Point CC/CXX at the MSVC or clang-cl driver the job asked
# for. clang-cl is the pinned LLVM's, not the image's older
# one on PATH; that prefix is installed by a later step.
set -euo pipefail

cc=cl
if [ "$COMPILER" = "clang-cl" ]; then
  cc="$(cygpath -m "$SPP_LLVM_WIN_PREFIX")/bin/clang-cl.exe"
fi
{
  echo "CC=$cc"
  echo "CXX=$cc"
} >> "$GITHUB_ENV"
