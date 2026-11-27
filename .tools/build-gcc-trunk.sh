#!/usr/bin/env bash
set -euo pipefail

# Build GCC trunk from a source checkout and install it, for trying a GCC fix ahead of a release (upstream-watch).
# A full bootstrap of C, C++ and LTO in a fresh build directory; gmp, mpfr and mpc come from the system
# (libgmp-dev libmpfr-dev libmpc-dev).
#
#   .tools/build-gcc-trunk.sh
#
#   GCC_SRC     source checkout, already pulled   (default ~/gcc)
#   GCC_BUILD   build directory, wiped first      (default ~/gcc-build)
#   GCC_PREFIX  install prefix                    (default /usr/local/gcc-dev)
#   GCC_BOOT    bin directory of the compiler that builds stage 1 (default /usr/local/gcc-15.2/bin, else PATH's)
#   JOBS        parallel jobs                     (default nproc; lower it if WSL runs out of memory)

src="${GCC_SRC:-${HOME}/gcc}"
build="${GCC_BUILD:-${HOME}/gcc-build}"
prefix="${GCC_PREFIX:-/usr/local/gcc-dev}"
boot="${GCC_BOOT:-/usr/local/gcc-15.2/bin}"
jobs="${JOBS:-$(nproc)}"

[[ -x "${src}/configure" ]] || { echo "no GCC checkout at ${src}" >&2; exit 1; }
cc="gcc" cxx="g++"
[[ -x "${boot}/gcc" ]] && cc="${boot}/gcc" cxx="${boot}/g++"

echo "GCC $(git -C "${src}" log -1 --format='%h %cs') -> ${prefix}, built by ${cc}, -j${jobs}"

rm -rf "${build}"
mkdir -p "${build}"
cd "${build}"

"${src}/configure" --prefix="${prefix}" CC="${cc}" CXX="${cxx}" --enable-languages=c,c++,lto
make -j"${jobs}" 2>&1 | tee make.log
sudo make install

"${prefix}/bin/g++" --version | head -1
