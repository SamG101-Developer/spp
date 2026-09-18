#!/usr/bin/env bash
# Run the project's CMake configure step into the `build`
# directory. Every conditional flag is decided here rather
# than in the workflows.
set -euo pipefail

args=()

# SPP_NO_COMPILER_LAUNCHER is set when the compiler cache is
# off (CodeQL), which forces the compiler to be invoked
# directly. Windows gets sccache, which can drive cl and
# clang-cl; everything else gets ccache.
if [ -z "${SPP_NO_COMPILER_LAUNCHER:-}" ]; then
  if [ "$RUNNER_OS" = "Windows" ]; then LAUNCHER=sccache; else LAUNCHER=ccache; fi
  args+=(-DCMAKE_C_COMPILER_LAUNCHER="$LAUNCHER" -DCMAKE_CXX_COMPILER_LAUNCHER="$LAUNCHER")
fi

# SPP_SANITIZER reuses the Debug profile with extra flags.
if [ -n "$SANITIZER" ]; then
  args+=(-DSPP_SANITIZER="$SANITIZER")
fi

# macOS needs the SDK spelled out to unlock the macros that
# otherwise block type definitions.
if [ "$RUNNER_OS" = "macOS" ]; then
  args+=(-DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)")
fi

# Ubuntu injects -D_FORTIFY_SOURCE=3, which trips a GCC 16
# ICE and fails the whole build.
FORTIFY_OFF="-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0"

# Ninja lives in a per-run RUNNER_TEMP directory, so its
# absolute path changes every run. A restored build tree
# still names the previous run's path in CMAKE_MAKE_PROGRAM,
# and CMake runs that dead path from project() before it
# looks at PATH.
if ! NINJA="$(command -v ninja)"; then
  echo "configure: ninja is not on PATH; setup-toolchain must run before this step" >&2
  exit 1
fi

# Git Bash reports an MSYS path (/c/...) that CMake cannot
# execute; -m gives the mixed C:/... form.
if [ "$RUNNER_OS" = "Windows" ]; then
  NINJA="$(cygpath -m "$NINJA")"
fi
args+=(-DCMAKE_MAKE_PROGRAM="$NINJA")

# Ninja is required for the C++ module support.
run_configure() {
  # shellcheck disable=SC2086
  cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_FLAGS="$FORTIFY_OFF" \
    -DCMAKE_CXX_FLAGS="$FORTIFY_OFF" \
    -DSPP_WERROR=ON \
    -DSPP_BUILD_TESTS=ON \
    -DSPP_USE_DEV_RPATH=OFF \
    "${args[@]}" \
    $EXTRA_FLAGS
}

# A restored tree carries the results of every compile check
# the last configure ran, as entries that are only computed
# once. One bad configure - a half-installed toolchain, a
# compiler not yet on PATH - therefore fails every later run
# identically and with no output, because the failed check
# is read from the cache instead of redone.
restored=false
if [ -f build/CMakeCache.txt ]; then
  restored=true
fi

if run_configure; then
  exit 0
fi

if [ "$restored" = true ]; then
  echo "configure: failed against the restored build tree; retrying from a clean one" >&2
  rm -rf build
  if run_configure; then
    exit 0
  fi
fi

# Where a find_package() that failed on a compile check says
# what actually went wrong.
log="build/CMakeConfigureLog.yaml"
if [ -f "$log" ]; then
  echo "configure: last 300 lines of ${log}" >&2
  tail -n 300 "$log" >&2
fi
exit 1
