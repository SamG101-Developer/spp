#!/usr/bin/env bash
# The prefix holding the LLVM development libraries the project
# links against. Deliberately not the compiler's prefix: on macOS
# the two are different LLVM releases, because brew has no llvm@23.
# shellcheck shell=bash

llvm_prefix() {
  case "$RUNNER_OS" in
    Windows) cygpath -m "$SPP_LLVM_WIN_PREFIX" ;;
    macOS) printf '%s\n' "$SPP_LLVM_MAC_PREFIX" ;;
    *) printf '%s\n' "/usr/lib/llvm-${LLVM_LIB_VERSION}" ;;
  esac
}
