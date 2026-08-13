#!/usr/bin/env bash
# The header/CMake dependencies: check each out at a pinned commit and install it into SPP_LOCAL_PREFIX, which the
# calling step caches as a single tree. This is for all libraries except for LLVM and Boost (different installation).
#
# Bumping: run .github/scripts/security/refresh-pins.sh, which rewrites the shas below to each repository's current
# default HEAD, then review and commit the diff.
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

# The cmake install helper checks the repo out at the given commit, configures it with the provided flags (typically
# disable the tests / benchmarks), builds it with Ninja, and installs it into the system.
cmake_install() {
  local url="$1" sha="$2"
  shift 2
  local name
  name="$(basename "$url" .git)"

  # A pinned checkout rather than a branch clone. GitHub will serve any reachable commit to a depth-1 fetch, so this
  # costs the same as the shallow clone it replaces while naming exactly what gets built.
  git init -q "$name"
  git -C "$name" remote add origin "$url"
  git -C "$name" fetch -q --depth 1 origin "$sha"
  git -C "$name" checkout -q --detach FETCH_HEAD
  echo "${name} @ ${sha}"

  cmake -S "$name" -B "$name/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_TESTING=OFF "$@"
  cmake --build "$name/build" --target install
}

# Install all the libraries from github that are cmake compatible.
cmake_install https://github.com/martinus/unordered_dense.git c4d143b6bbe7c4b2b5700fc26c96ef90d11cb66e
cmake_install https://github.com/microsoft/mimalloc.git fc1e2acbced0b3e893da1a1375e02ac159d0423f \
  -DMI_SECURE=OFF
cmake_install https://github.com/nlohmann/json.git cdf52ae9bef77a0844e02e42df6d2df83a55c4b9 \
  -DJSON_BuildTests=OFF
cmake_install https://github.com/marzer/tomlplusplus.git 1e8829b793b66ad17011732a146b8077d379b011
cmake_install https://github.com/Neargye/magic_enum.git 591b64351ea8442f8b8fa044ff335d6943e8e6e0 \
  -DMAGIC_ENUM_OPT_BUILD_TESTS=OFF -DMAGIC_ENUM_OPT_BUILD_EXAMPLES=OFF
cmake_install https://github.com/CLIUtils/CLI11.git 60492bddb50422f32cfa33c1365b96ebee4205ca \
  -DCLI11_BUILD_TESTS=OFF -DCLI11_BUILD_EXAMPLES=OFF
cmake_install https://github.com/ericniebler/range-v3.git 108f93c279c8f9cec175dac361084983d0176e99 \
  -DRANGE_V3_TESTS=OFF -DRANGE_V3_EXAMPLES=OFF -DRANGE_V3_DOCS=OFF
cmake_install https://github.com/SamG101-Developer/googletest.git 2dc53710d79c49d58bc3d7bdbfc5c0e09cef1361
cmake_install https://github.com/SamG101-Developer/ColEx.git d84ca4f8dd153086342c719fc3ebad53c73faa42
cmake_install https://github.com/SamG101-Developer/GenEx.git 50e0530f47de8bda75dcc76fb0206728370d5d48
