#!/usr/bin/env bash
# The header/CMake dependencies: clone each at its default branch and install it into SPP_LOCAL_PREFIX, which the
# calling step caches as a single tree. This is for all libraries except for LLVM and Boost (different installation).
set -euo pipefail

PREFIX="$SPP_LOCAL_PREFIX"
mkdir -p "$PREFIX" "${RUNNER_TEMP}/libs"
cd "${RUNNER_TEMP}/libs"

if [ "$RUNNER_OS" = "Windows" ]; then
  PREFIX="$(cygpath -m "$PREFIX")"
  llvm_prefix="$(cygpath -m "$SPP_LLVM_WIN_PREFIX")"
  export CMAKE_PREFIX_PATH="$PREFIX;${BOOST_ROOT};$llvm_prefix"
else
  llvm_prefix="${LLVM_PREFIX:-/usr/lib/llvm-${LLVM_LIB_VERSION}}"
  export CMAKE_PREFIX_PATH="$PREFIX${BOOST_ROOT:+:$BOOST_ROOT}:$llvm_prefix"
fi

# The cmake install helper clones the repo, configured it with the provided flags (typically disable the tests /
# benchmarks), builds it with Ninja, and installs it into the system.
cmake_install() {
  local url="$1"; shift
  local name; name="$(basename "$url" .git)"
  git clone --depth 1 "$url" "$name"
  cmake -S "$name" -B "$name/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_TESTING=OFF "$@"
  cmake --build "$name/build" --target install
}

# Install all the libraries from github that are cmake compatible.
cmake_install https://github.com/martinus/unordered_dense.git
cmake_install https://github.com/microsoft/mimalloc.git           -DMI_SECURE=OFF
cmake_install https://github.com/nlohmann/json.git                -DJSON_BuildTests=OFF
cmake_install https://github.com/marzer/tomlplusplus.git
cmake_install https://github.com/Neargye/magic_enum.git           -DMAGIC_ENUM_OPT_BUILD_TESTS=OFF -DMAGIC_ENUM_OPT_BUILD_EXAMPLES=OFF
cmake_install https://github.com/CLIUtils/CLI11.git               -DCLI11_BUILD_TESTS=OFF -DCLI11_BUILD_EXAMPLES=OFF
cmake_install https://github.com/ericniebler/range-v3.git         -DRANGE_V3_TESTS=OFF -DRANGE_V3_EXAMPLES=OFF -DRANGE_V3_DOCS=OFF
cmake_install https://github.com/SamG101-Developer/googletest.git
cmake_install https://github.com/SamG101-Developer/ColEx.git
cmake_install https://github.com/SamG101-Developer/OpEx.git
cmake_install https://github.com/SamG101-Developer/GenEx.git
