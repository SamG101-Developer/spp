#!/usr/bin/env bash
# Build the pinned cppcheck into SPP_CPPCHECK_PREFIX, which
# the calling step caches as a single tree.
#
# Bumping: run .github/scripts/security/refresh-pins.sh.
set -euo pipefail

PREFIX="$SPP_CPPCHECK_PREFIX"
src="${RUNNER_TEMP}/cppcheck"

mkdir -p "$PREFIX"

# Same pinned-checkout shape as install-small-libs.sh: a
# depth-1 fetch of one named commit costs what a shallow
# clone costs, while naming exactly what gets built.
git init -q "$src"
git -C "$src" remote add origin https://github.com/cppcheck-opensource/cppcheck.git
git -C "$src" fetch -q --depth 1 origin "$CPPCHECK_COMMIT"
git -C "$src" checkout -q --detach FETCH_HEAD
echo "cppcheck ${CPPCHECK_VERSION} @ ${CPPCHECK_COMMIT}"

# FILESDIR is where cppcheck looks for its own configuration
# at runtime, so it has to be baked in rather than left to
# the working directory the analysis happens to run from.
cmake -S "$src" -B "$src/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DFILESDIR="$PREFIX/share/Cppcheck" \
  -DUSE_MATCHCOMPILER=ON \
  -DBUILD_TESTS=OFF \
  -DBUILD_GUI=OFF

cmake \
  --build "$src/build" \
  --target install \
  --parallel
