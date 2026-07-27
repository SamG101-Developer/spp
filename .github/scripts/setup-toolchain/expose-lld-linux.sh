#!/usr/bin/env bash
# CMAKE_LINKER_TYPE=LLD makes GCC pass -fuse-ld=lld, and GCC only ever looks for a plain 'ld.lld' on PATH.
set -euo pipefail

sudo apt-get install -y "lld-${LLVM_LIB_VERSION}"
sudo ln -sf "/usr/lib/llvm-${LLVM_LIB_VERSION}/bin/ld.lld" /usr/bin/ld.lld
ld.lld --version
