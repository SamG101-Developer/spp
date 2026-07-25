#!/usr/bin/env bash
# Install the requested Clang from brew, point CC/CXX at it, and rewrite the libc++ module manifest with absolute paths
# so module builds resolve it.
set -euo pipefail

formula="llvm${CLANG_VERSION:+@${CLANG_VERSION}}"
brew install "$formula"
prefix="$(brew --prefix "$formula")"
{
  echo "CC=${prefix}/bin/clang"
  echo "CXX=${prefix}/bin/clang++"
  echo "LDFLAGS=-L${prefix}/lib/c++ -Wl,-rpath,${prefix}/lib/c++"
  echo "LLVM_PREFIX=${prefix}"
} >> "$GITHUB_ENV"
resource_dir="$("${prefix}/bin/clang" -print-resource-dir)"
sed "s|\"\.\./\.\./|\"${prefix}/|g" \
  "${prefix}/lib/c++/libc++.modules.json" > "${resource_dir}/libc++.modules.json"
