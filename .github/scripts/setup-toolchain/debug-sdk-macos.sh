#!/usr/bin/env bash
# Dump what the failing macOS build was actually doing. Synthetic
# include probes are ruled out: a bare stdio.h, the sys.ixx include
# order, and that order in a global module fragment all compile
# clean under both the CommandLineTools sysroot brew clang pins and
# the Xcode sysroot CMake passes, the two SDK trees are identical on
# every header in the failure path, and a cold build tree fails the
# same way. So the difference is in the real compile's flags.
#
# Reporting only: runs after a failed build and never fails itself.
set -uo pipefail

CXX="${CXX:-c++}"

section() {
  echo "::endgroup::"
  echo "::group::$1"
}

echo "::group::toolchain and SDK"
echo "SPP_RUNNER_IMAGE=${SPP_RUNNER_IMAGE:-<unset>}"
xcode-select -p
xcrun --show-sdk-path
"$CXX" --version | head -n 1
"$CXX" -v 2>&1 | sed -n 's/^Configuration file: /config: /p'

# The macOS 11.3 / clang 11 failure (Mozilla 1708034, MacPorts
# 62770) is two c++/v1 directories in one search path: libc++'s
# stddef.h does #include_next <stddef.h>, a same-guard copy in the
# SDK swallows it, and the chain never reaches the clang header
# that typedefs size_t. The default sysroot here lists only brew's
# c++/v1 - but CMake compiles under -isysroot, where the driver
# recomputes these paths. So print both and count.
section "c++/v1 directories, default sysroot vs CMake's -isysroot"
xcode_sdk="$(xcrun --show-sdk-path)"
echo "--- SDK ships its own libc++?"
ls -d "${xcode_sdk}/usr/include/c++/v1" 2>/dev/null || echo "(no ${xcode_sdk}/usr/include/c++/v1)"
for mode in default isysroot; do
  echo "--- ${mode}"
  if [ "$mode" = default ]; then
    order="$("$CXX" -std=c++26 -E -x c++ -v /dev/null 2>&1)"
  else
    order="$("$CXX" -std=c++26 -isysroot "$xcode_sdk" -E -x c++ -v /dev/null 2>&1)"
  fi
  printf '%s\n' "$order" | sed -n '/#include </,/End of search list/p'
  echo "c++/v1 entries: $(printf '%s\n' "$order" \
    | sed -n '/#include </,/End of search list/p' | grep -c 'c++/v1')"
done

section "what configure put in the cache"
if [ -f build/CMakeCache.txt ]; then
  grep -E '^CMAKE_(OSX_SYSROOT|OSX_DEPLOYMENT_TARGET|CXX_COMPILER|CXX_FLAGS|CXX_STANDARD|CXX_COMPILER_LAUNCHER|CXX_MODULE_STD):' \
    build/CMakeCache.txt
else
  echo "(no build/CMakeCache.txt)"
fi

# The full command line for the failing TU, and every flag on it.
# This is the piece no probe can reconstruct: -D_FORTIFY_SOURCE=0,
# the warning set, the module flags and the include order all land
# here, and one of them is what the probes were missing.
#
# Ninja retries the failed edge, so this prints the command and the
# diagnostic from its start - including the "In file included from"
# chain that "too many errors emitted" truncated away.
section "the failing compile, verbatim"
cmake --build build --verbose 2>&1 | head -n 300

echo "::endgroup::"
exit 0
