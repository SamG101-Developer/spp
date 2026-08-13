#!/usr/bin/env bash
# Fetch the apt.llvm.org installer the Clang and LLVM steps run later.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

verified_fetch https://apt.llvm.org/llvm.sh "${RUNNER_TEMP}/llvm.sh" "$LLVM_SH_SHA256"
chmod +x "${RUNNER_TEMP}/llvm.sh"
