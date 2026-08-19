#!/usr/bin/env bash
# macOS takes the LLVM development libraries from the official
# release tarball rather than from brew: the newest formula is
# 22.x, a major version behind the LLVM this project links
# against, and there is no llvm@23 to install instead. The
# compiler stays the brew clang, whose libc++ ships the module
# manifest that "import std" needs.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

root="${LLVM_MAC_ASSET%.tar.xz}"
tarball="${RUNNER_TEMP}/llvm.tar.xz"
prefix="$SPP_LLVM_MAC_PREFIX"
exported="${RUNNER_TEMP}/llvm-exported.txt"
files="${RUNNER_TEMP}/llvm-files.txt"

url="https://github.com/llvm/llvm-project/releases/download/${LLVM_MAC_TAG}/${LLVM_MAC_ASSET}"
verified_fetch "$url" "$tarball" "$LLVM_MAC_SHA256"
mkdir -p "$prefix"

# Unpacked whole, the tarball is 8.2 GB of which most is clang,
# mlir and lldb tools nothing here links against; the runner has
# 14 GB of disk and the cache has to hold a copy as well. So
# unpack the CMake package first and take the file list from it:
# LLVMExports names every file find_package(LLVM) checks for -
# a missing one is a fatal configure error - and nothing it does
# not name is needed.
tar xf "$tarball" -C "$prefix" --strip-components=1 "${root}/lib/cmake/llvm"
grep -o '_IMPORT_PREFIX}/[^"]*' "${prefix}/lib/cmake/llvm/LLVMExports-release.cmake" \
  | sed 's|_IMPORT_PREFIX}/||' | sort -u > "$exported"

# The libraries and headers the project compiles and links
# against: 0.7 GB of the 2.9 GB the export file names.
{
  echo "include/llvm"
  echo "include/llvm-c"
  grep -v '^bin/' "$exported"
} | sed "s|^|${root}/|" > "$files"
tar xf "$tarball" -C "$prefix" --strip-components=1 -T "$files"

# The other 2.2 GB is the 74 exported tools - opt, llc, dsymutil,
# ... - statically linked and none of them run here. All the
# import check asks is that the path EXISTS, so empty files
# satisfy it, and anything that did try to run one fails loudly.
mkdir -p "${prefix}/bin"
while read -r tool; do
  : > "${prefix}/${tool}"
done < <(grep '^bin/' "$exported")
rm -f "$tarball"
