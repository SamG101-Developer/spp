#!/usr/bin/env bash
# Install the requested Clang, plus the libstdc++ it
# borrows, and point CC/CXX at it.
set -euo pipefail
source .github/scripts/lib/retry.sh

retry sudo "${RUNNER_TEMP}/llvm.sh" "${CLANG_VERSION}"
retry sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
retry sudo apt-get update
retry sudo apt-get install -y "g++-${GCC_VERSION%%.*}"
{
  echo "CC=clang-${CLANG_VERSION}"
  echo "CXX=clang++-${CLANG_VERSION}"
} >> "$GITHUB_ENV"
