#!/usr/bin/env bash
# Install clang-tidy at the pinned LLVM version, from the
# apt.llvm.org repository that install-clang-linux.sh added.
set -euo pipefail
source .github/scripts/lib/retry.sh

retry sudo apt-get install -y "clang-tidy-${LLVM_LIB_VERSION}"
"clang-tidy-${LLVM_LIB_VERSION}" --version
