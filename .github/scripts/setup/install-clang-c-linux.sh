#!/usr/bin/env bash
# Install the pinned LLVM major's Clang, as a C compiler
# only: the run-time benchmarks' C programs need nothing
# else, so unlike install-clang-linux.sh no libstdc++.
set -euo pipefail

sudo "${RUNNER_TEMP}/llvm.sh" "${LLVM_LIB_VERSION}"
"clang-${LLVM_LIB_VERSION}" --version
