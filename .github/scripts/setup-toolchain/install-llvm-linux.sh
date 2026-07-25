#!/usr/bin/env bash
# Install the LLVM development libraries the project links against.
set -euo pipefail

# Already installed if this job's compiler *is* this LLVM release.
if ! [ -d "/usr/lib/llvm-${LLVM_LIB_VERSION}" ]; then
  sudo "${RUNNER_TEMP}/llvm.sh" "${LLVM_LIB_VERSION}"
fi
sudo apt-get install -y "llvm-${LLVM_LIB_VERSION}-dev"
