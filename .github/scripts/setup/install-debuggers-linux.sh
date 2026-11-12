#!/usr/bin/env bash
# What the codegen checks read a program with: LLVM's lldb, with
# its Python for the std's formatters, and llvm-dwarfdump and
# llvm-symbolizer, at the LLVM release spp is built against,
# from apt.llvm.org (llvm.sh, fetched and verified beforehand);
# and what gdb, built separately and cached, needs to build and
# to run: GMP, MPFR and the system Python's library.
set -euo pipefail
source .github/scripts/lib/retry.sh

retry sudo apt-get update
retry sudo apt-get install -y libgmp-dev libmpfr-dev python3-dev

retry sudo "${RUNNER_TEMP}/llvm.sh" "${LLVM_LIB_VERSION}"
retry sudo apt-get install -y "llvm-${LLVM_LIB_VERSION}" "python3-lldb-${LLVM_LIB_VERSION}"

"lldb-${LLVM_LIB_VERSION}" --version
"lldb-${LLVM_LIB_VERSION}" -b -o 'script import lldb; print("lldb python", lldb.SBDebugger.GetVersionString().split()[2])'
"/usr/lib/llvm-${LLVM_LIB_VERSION}/bin/llvm-dwarfdump" --version | head -2
