#!/usr/bin/env bash
# Build the pinned cppcheck into SPP_CPPCHECK_PREFIX. the newest version must be pulled and built; the ubuntu apt install
# version is too old, because the they don't have module support, which is a hard requirement.
set -euo pipefail

# Clone the cppcheck repo into its own isolated directory in the temp area, and ensure we are on the pinned version (by
# hash tag. Dependabot will keep this upto date.
git clone https://github.com/danmar/cppcheck.git "${RUNNER_TEMP}/cppcheck-src"
git -C "${RUNNER_TEMP}/cppcheck-src" checkout --detach "$CPPCHECK_SHA"

# Configure the cppcheck build, using the MATCH_COMPILER flag as this roughly halves analysis time. Build and install
# the project into the given directory.
cmake -S "${RUNNER_TEMP}/cppcheck-src" -B "${RUNNER_TEMP}/cppcheck-build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$SPP_CPPCHECK_PREFIX" \
  -DUSE_MATCHCOMPILER=ON \
  -DBUILD_TESTS=OFF \
  -GNinja
cmake --build "${RUNNER_TEMP}/cppcheck-build" --parallel
cmake --install "${RUNNER_TEMP}/cppcheck-build"
