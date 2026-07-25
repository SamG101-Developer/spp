#!/usr/bin/env bash
# Run the project's CMake configure step into the `build` directory. Handle all conditional flags and configurations in
# this script.
set -euo pipefail

args=()

# The SPP_NO_COMPILER_LAUNCHER flag is set when the compiler cache is off (CodeQL), forcing the compiler to be invoked
# directly. Otherwise, the cache configuration is added; sccache or ccache. The flags go into the cmake configuration.
if [ -z "${SPP_NO_COMPILER_LAUNCHER:-}" ]; then
  if [ "$RUNNER_OS" = "Windows" ]; then LAUNCHER=sccache; else LAUNCHER=ccache; fi
  args+=(-DCMAKE_C_COMPILER_LAUNCHER="$LAUNCHER" -DCMAKE_CXX_COMPILER_LAUNCHER="$LAUNCHER")
fi

# For a sanitizer build, append the flags into the SPP_SANITIZER option, which is read in the CMakeLists.txt file. This
# reuses the Debug profile with additional args.
if [ -n "$SANITIZER" ]; then
  args+=(-DSPP_SANITIZER="$SANITIZER")
fi

# Ubuntu injects -D_FORTIFY_SOURCE=3, which triggers a GCC 16 ICE. Disable it otherwise the entire cmake build will
# fail. Don't think it's an issue on GCC 17 but runner must use GCC 16.
FORTIFY_OFF="-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"

# Launch the cmake configuration script into the "build" folder. Ninja must be used for the c++ module support.
# shellcheck disable=SC2086  # EXTRA_FLAGS is a deliberate word-split flag list passed in from the calling action.
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_C_FLAGS="$FORTIFY_OFF" \
  -DCMAKE_CXX_FLAGS="$FORTIFY_OFF" \
  -DSPP_WERROR=ON \
  -DSPP_BUILD_TESTS=ON \
  -DSPP_USE_DEV_RPATH=OFF \
  "${args[@]}" \
  $EXTRA_FLAGS
