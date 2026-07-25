#!/usr/bin/env bash
# Install the requested Clang, plus the libstdc++ it borrows, and point CC/CXX at it.
set -euo pipefail

sudo "${RUNNER_TEMP}/llvm.sh" "${CLANG_VERSION}"
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install -y "g++-${LIBSTDCXX_GCC_VERSION}"
echo "CC=clang-${CLANG_VERSION}" >> "$GITHUB_ENV"
echo "CXX=clang++-${CLANG_VERSION}" >> "$GITHUB_ENV"
