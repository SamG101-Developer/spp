#!/usr/bin/env bash
# Point CC/CXX at the clang that install-llvm-macos.sh unpacked - the
# same release the project links against, see the comment there - and
# rewrite the libc++ module manifest with absolute paths so module
# builds resolve it.
set -euo pipefail

prefix="$SPP_LLVM_MAC_PREFIX"

# There is nothing to install a different version from: the compiler
# is whichever one [pin.llvm-mac] names.
pinned="${LLVM_MAC_TAG#llvmorg-}"
pinned="${pinned%%.*}"
if [ -n "${CLANG_VERSION:-}" ] && [ "$CLANG_VERSION" != "$pinned" ]; then
  echo "::error::clang ${CLANG_VERSION} was asked for, but macOS builds with the pinned LLVM ${pinned}" >&2
  exit 1
fi

{
  echo "CC=${prefix}/bin/clang"
  echo "CXX=${prefix}/bin/clang++"
  echo "LDFLAGS=-L${prefix}/lib -Wl,-rpath,${prefix}/lib"
} >> "$GITHUB_ENV"

# The manifest's source paths are relative to its own directory, which
# sits either in lib/ or lib/c++/ depending on how the release laid
# libc++ out; rewriting from the "share/libc++/v1" they all end in is
# what makes that immaterial.
manifest="$(find "${prefix}/lib" -maxdepth 2 -name libc++.modules.json | head -n 1)"
if [ -z "$manifest" ]; then
  echo "::error::no libc++.modules.json under ${prefix}/lib; 'import std' has nothing to build from" >&2
  exit 1
fi
resource_dir="$("${prefix}/bin/clang" -print-resource-dir)"
sed "s|\"[^\"]*share/libc++/v1/|\"${prefix}/share/libc++/v1/|g" \
  "$manifest" > "${resource_dir}/libc++.modules.json"
