#!/usr/bin/env bash
# The prebuilt LLVM for Windows is built against zlib, zstd and
# libxml2, so LLVMExports.cmake gives LLVMSupport a ZLIB::ZLIB
# and a zstd::libzstd_static, and LLVMWindowsManifest a
# LibXml2::LibXml2. LLVMConfig.cmake only tries to resolve those
# against the paths of the machine that built the release, and
# the runner image carries none of the three, so find_package(LLVM)
# dies on the undefined targets while reading the export file -
# whether or not this project links the target that wants them.
# Build all three into one prefix, which expose-prefixes.sh puts
# on CMAKE_PREFIX_PATH.
set -euo pipefail

prefix="$(cygpath -m "$SPP_LLVM_DEPS_WIN_PREFIX")"
work="${RUNNER_TEMP}/llvm-deps"

rm -rf "$work"
mkdir -p "$work"

# Check out at the pinned commit, configure, build, install.
# The same shape as install-small-libs.sh, without the manifest
# loop: these three are Windows-only, and each one needs its own
# flags and its own source directory.
build() {
  local name="$1" url="$2" sha="$3" src="$4"
  shift 4

  git init -q "${work}/${name}"
  git -C "${work}/${name}" remote add origin "$url"
  git -C "${work}/${name}" fetch -q --depth 1 origin "$sha"
  git -C "${work}/${name}" checkout -q --detach FETCH_HEAD
  echo "${name} @ ${sha}"

  # Release against the dynamic CRT, which is what the prebuilt
  # LLVM these link beside was built with.
  cmake -S "${work}/${name}/${src}" -B "${work}/${name}/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF "$@"
  cmake --build "${work}/${name}/build" --target install
}

build zlib https://github.com/madler/zlib.git "$ZLIB_COMMIT" . \
  -DZLIB_BUILD_EXAMPLES=OFF

# zlib builds both flavours whatever it is asked for, and
# FindZLIB picks zlib.lib, the import library for zlib1.dll,
# which would leave every binary built here needing that DLL
# beside it at run time. Keep the static one, under the name
# FindZLIB looks for.
rm -f "${prefix}/bin/zlib1.dll" "${prefix}/lib/zlib.lib"
mv "${prefix}/lib/zlibstatic.lib" "${prefix}/lib/zlib.lib"

build zstd https://github.com/facebook/zstd.git "$ZSTD_COMMIT" build/cmake \
  -DZSTD_BUILD_SHARED=OFF \
  -DZSTD_BUILD_STATIC=ON \
  -DZSTD_BUILD_PROGRAMS=OFF \
  -DZSTD_BUILD_TESTS=OFF \
  -DZSTD_LEGACY_SUPPORT=OFF

# ICONV is on by default and looked up as REQUIRED, and there is
# no iconv on a Windows runner; the rest is xmllint and bindings
# nothing here reads.
build libxml2 https://github.com/GNOME/libxml2.git "$LIBXML2_COMMIT" . \
  -DLIBXML2_WITH_ICONV=OFF \
  -DLIBXML2_WITH_MODULES=OFF \
  -DLIBXML2_WITH_PROGRAMS=OFF \
  -DLIBXML2_WITH_PYTHON=OFF \
  -DLIBXML2_WITH_TESTS=OFF

# None of this is worth having if find_package() cannot pick it
# up, and a missing piece reads far better here than as an
# undefined target 1000 lines into LLVMExports.cmake.
for path in \
  include/zlib.h \
  lib/zlib.lib \
  lib/cmake/zstd/zstdConfig.cmake \
  lib/cmake/libxml2/libxml2-config.cmake; do
  if [ ! -f "${prefix}/${path}" ]; then
    echo "::error::install-llvm-deps-windows: ${path} is missing from ${prefix}"
    exit 1
  fi
done
