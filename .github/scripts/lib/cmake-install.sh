# Configure, build and install one CMake project in Release with its tests off.
# shellcheck shell=bash

# cmake_install <source-dir> <build-dir> <install-prefix> [cmake flags...]
cmake_install() {
  local src="$1" build="$2" prefix="$3"
  shift 3

  cmake -S "$src" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_TESTING=OFF "$@"
  cmake --build "$build" --target install --parallel
}
