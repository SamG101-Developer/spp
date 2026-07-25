#!/usr/bin/env bash
# Fetch the apt.llvm.org installer the Clang and LLVM steps run later.
set -euo pipefail

curl -fsSL -o "${RUNNER_TEMP}/llvm.sh" https://apt.llvm.org/llvm.sh
chmod +x "${RUNNER_TEMP}/llvm.sh"
