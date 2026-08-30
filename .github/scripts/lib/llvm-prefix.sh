#!/usr/bin/env bash
# The prefix holding the LLVM development libraries the project
# links against. On macOS it holds the compiler as well, unpacked
# from the same release: brew has no llvm@23 to compile with.
# shellcheck shell=bash

llvm_prefix() {
  case "$RUNNER_OS" in
    Windows) cygpath -m "$SPP_LLVM_WIN_PREFIX" ;;
    macOS) printf '%s\n' "$SPP_LLVM_MAC_PREFIX" ;;
    *) printf '%s\n' "/usr/lib/llvm-${LLVM_LIB_VERSION}" ;;
  esac
}
