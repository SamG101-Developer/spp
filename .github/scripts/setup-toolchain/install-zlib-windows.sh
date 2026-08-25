#!/usr/bin/env bash
# The Windows LLVM release is built with zlib enabled, so
# LLVMExports.cmake gives LLVMSupport a ZLIB::ZLIB link
# dependency. LLVMConfig.cmake only tries to satisfy it with a
# non-REQUIRED find_package(ZLIB) hinted at the ZLIB_ROOT of the
# machine that built the release, and the runner image ships no
# zlib of its own, so the lookup silently finds nothing and
# find_package(LLVM) dies inside LLVMExports.cmake on the
# missing target. Build a zlib and install it where the lookup
# will find it (expose-prefixes.sh puts this prefix on
# CMAKE_PREFIX_PATH).
set -euo pipefail

prefix="$(cygpath -m "$SPP_ZLIB_WIN_PREFIX")"
src="${RUNNER_TEMP}/zlib"

rm -rf "$src"
git init -q "$src"
git -C "$src" remote add origin https://github.com/madler/zlib.git
git -C "$src" fetch -q --depth 1 origin "$ZLIB_COMMIT"
git -C "$src" checkout -q --detach FETCH_HEAD
echo "zlib ${ZLIB_VERSION} @ ${ZLIB_COMMIT}"

# Release and the dynamic CRT, which is what the prebuilt LLVM
# this links beside was built with.
cmake -S "$src" -B "$src/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
  -DZLIB_BUILD_EXAMPLES=OFF
cmake --build "$src/build" --target install

# zlib installs both libraries and FindZLIB picks zlib.lib, the
# import library for zlib1.dll, which would leave every binary
# built here needing that DLL beside it at run time. Keep the
# static one, under the name FindZLIB looks for.
rm -f "$prefix/bin/zlib1.dll" "$prefix/lib/zlib.lib"
mv "$prefix/lib/zlibstatic.lib" "$prefix/lib/zlib.lib"
