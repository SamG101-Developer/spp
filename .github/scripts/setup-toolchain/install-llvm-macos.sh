#!/usr/bin/env bash
# macOS takes its whole toolchain from the official LLVM release
# tarball rather than from brew.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

root="${LLVM_MAC_ASSET%.tar.xz}"
tarball="${RUNNER_TEMP}/llvm.tar.xz"
prefix="$SPP_LLVM_MAC_PREFIX"
listing="${RUNNER_TEMP}/llvm-listing.txt"
exported="${RUNNER_TEMP}/llvm-exported.txt"
files="${RUNNER_TEMP}/llvm-files.txt"

url="https://github.com/llvm/llvm-project/releases/download/${LLVM_MAC_TAG}/${LLVM_MAC_ASSET}"
verified_fetch "$url" "$tarball" "$LLVM_MAC_SHA256"
mkdir -p "$prefix"

# Unpacked whole, the tarball is 8.2 GB of which most is mlir, lldb
# and tools nothing here runs; the runner has 14 GB of disk and the
# cache has to hold a copy as well. So take a listing of the archive
# and unpack only the members named below.
tar tf "$tarball" > "$listing"

# The library side comes from the CMake package: LLVMExports names
# every file find_package(LLVM) checks for - a missing one is a fatal
# configure error - and nothing it does not name is needed.
tar xf "$tarball" -C "$prefix" --strip-components=1 "${root}/lib/cmake/llvm"
grep -o '_IMPORT_PREFIX}/[^"]*' "${prefix}/lib/cmake/llvm/LLVMExports-release.cmake" \
  | sed 's|_IMPORT_PREFIX}/||' | sort -u > "$exported"

grep -E \
  -e "^${root}/bin/clang(\+\+|-[0-9]+|-scan-deps)?$" \
  -e "^${root}/(lib/clang/|include/c\+\+/|lib/c\+\+/|lib/libc\+\+|lib/libunwind|lib/libLTO\.|share/libc\+\+/)" \
  "$listing" > "${files}.compiler"

{
  echo "${root}/include/llvm"
  echo "${root}/include/llvm-c"
  grep -v '^bin/' "$exported" | sed "s|^|${root}/|"
  cat "${files}.compiler"
} | sort -u > "$files"
tar xf "$tarball" -C "$prefix" --strip-components=1 -T "$files"

mkdir -p "${prefix}/bin"
while read -r tool; do
  [ -e "${prefix}/${tool}" ] || : > "${prefix}/${tool}"
done < <(grep '^bin/' "$exported")
rm -f "$tarball"

# A piece missing here surfaces much later as something inscrutable -
# a configure that falls back to the system clang, a link that cannot
# read the archives - so fail now, while the cause is still on screen.
for needed in bin/clang++ bin/clang-scan-deps lib/libLTO.dylib; do
  if [ ! -e "${prefix}/${needed}" ]; then
    echo "::error::${LLVM_MAC_ASSET} has no ${needed}; macOS cannot build against this release" >&2
    exit 1
  fi
done
"${prefix}/bin/clang++" --version
