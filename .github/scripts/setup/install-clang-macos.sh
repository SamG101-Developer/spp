#!/usr/bin/env bash
# Point CC/CXX at the clang that install-llvm-macos.sh installed - the
# same brew keg the project links against, see the comment there - and
# rewrite the libc++ module manifest with absolute paths so module
# builds resolve it.
set -euo pipefail

prefix="$SPP_CI_LLVM_MAC_PREFIX"

# There is nothing to install a different version from: the compiler
# is the [pin.llvm-lib] major that the dev libraries are.
if [ -n "${CLANG_VERSION:-}" ] && [ "$CLANG_VERSION" != "$LLVM_LIB_VERSION" ]; then
  echo "::error::clang ${CLANG_VERSION} was asked for, but macOS builds with LLVM ${LLVM_LIB_VERSION}" >&2
  exit 1
fi

# libc++ is linked statically. As a dylib it is resolved at run time,
# and wherever the rpath misses - every user's machine, for a released
# binary - dyld silently binds /usr/lib's older libc++ instead, which
# disagrees with these headers on the layout of std::locale. Loaded
# hidden: the system libc++ is in every process anyway, and dyld
# coalesces exported weak symbols with it by name.
libcxx="$(find "${prefix}/lib" -maxdepth 2 -name libc++.a | head -n 1)"
libcxxabi="$(find "${prefix}/lib" -maxdepth 2 -name libc++abi.a | head -n 1)"
if [ -z "$libcxx" ] || [ -z "$libcxxabi" ]; then
  echo "::error::no libc++.a / libc++abi.a under ${prefix}/lib; libc++ cannot be linked statically" >&2
  exit 1
fi

# brew builds its bottles for the runner's macOS, so the static
# LLVM and libc++ archives set the floor; [floor] macos names it.
# CMake seeds CMAKE_OSX_DEPLOYMENT_TARGET from this, and the small
# libraries' builds read it too.
{
  echo "MACOSX_DEPLOYMENT_TARGET=${SPP_CI_FLOOR_MACOS}"
  echo "CC=${prefix}/bin/clang"
  echo "CXX=${prefix}/bin/clang++"
  echo "LDFLAGS=-nostdlib++ -Wl,-load_hidden,${libcxx} -Wl,-load_hidden,${libcxxabi}"
} >> "$GITHUB_ENV"

# The manifest's paths are relative to its own directory, which sits
# either in lib/ or lib/c++/ depending on how the build laid libc++
# out (brew's is lib/c++/), and they are read from the resource directory it is copied to
# rather than from there.
manifest="$(find "${prefix}/lib" -maxdepth 2 -name libc++.modules.json | head -n 1)"
if [ -z "$manifest" ]; then
  echo "::error::no libc++.modules.json under ${prefix}/lib; 'import std' has nothing to build from" >&2
  exit 1
fi
resource_dir="$("${prefix}/bin/clang" -print-resource-dir)"
sed "s|\"[^\"]*share/libc++/v1|\"${prefix}/share/libc++/v1|g" \
  "$manifest" > "${resource_dir}/libc++.modules.json"
