#!/usr/bin/env bash
# Build the pinned cppcheck into SPP_CPPCHECK_PREFIX,
# which the calling step caches as a single tree.
#
# Bumping: run .github/scripts/maint/refresh-pins.sh.
set -euo pipefail
source .github/scripts/lib/pinned-checkout.sh
source .github/scripts/lib/cmake-install.sh

src="${RUNNER_TEMP}/cppcheck"
mkdir -p "$SPP_CPPCHECK_PREFIX"

pinned_checkout "$src" https://github.com/cppcheck-opensource/cppcheck.git "$CPPCHECK_COMMIT"
echo "cppcheck ${CPPCHECK_VERSION}"

# FILESDIR is where cppcheck looks for its own configuration
# at runtime, so it has to be baked in rather than left to
# the working directory the analysis happens to run from.
cmake_install "$src" "$src/build" "$SPP_CPPCHECK_PREFIX" \
  -DFILESDIR="$SPP_CPPCHECK_PREFIX/share/Cppcheck" \
  -DUSE_MATCHCOMPILER=ON \
  -DBUILD_TESTS=OFF \
  -DBUILD_GUI=OFF
