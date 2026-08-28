#!/usr/bin/env bash
# Diagnose the "unknown type name 'size_t'" failure in the SDK's
# _stdio.h. Both clang's __stddef_size_t.h and the SDK's
# sys/_types/_size_t.h guard on _SIZE_T, so whichever lands first
# no-ops the other; the error means the macro was set without the
# typedef being in scope. These probes say which header won and in
# which of the three contexts it goes wrong.
#
# Reporting only: every probe is allowed to fail, so a broken
# toolchain still produces a full log instead of stopping at the
# first bad command.
set -uo pipefail

CXX="${CXX:-c++}"
probe="${RUNNER_TEMP:-/tmp}/sdk-probe"
mkdir -p "$probe"

section() {
  echo "::endgroup::"
  echo "::group::$1"
}

echo "::group::toolchain and SDK"
echo "SPP_RUNNER_IMAGE=${SPP_RUNNER_IMAGE:-<unset>}"
echo "SDKROOT=${SDKROOT:-<unset>}"
echo "CXX=${CXX}"
xcode-select -p
xcrun --show-sdk-path
xcrun --show-sdk-version
"$CXX" --version
"$CXX" -print-resource-dir

section "include search order"
"$CXX" -std=c++26 -E -x c++ -v /dev/null 2>&1 | sed -n '/#include </,/End of search list/p'

section "which size_t header wins"
sdk="$(xcrun --show-sdk-path)"
res="$("$CXX" -print-resource-dir)"
for f in "${res}/include/__stddef_size_t.h" "${sdk}/usr/include/sys/_types/_size_t.h"; do
  echo "--- ${f}"
  if [ -f "$f" ]; then grep -n '_SIZE_T\|typedef' "$f"; else echo "(missing)"; fi
done
echo "--- ${sdk}/usr/include/_stdio.h around the reported line"
sed -n '455,470p' "${sdk}/usr/include/_stdio.h" 2>/dev/null || echo "(no _stdio.h)"

# Probe 1: does the SDK's stdio.h stand up on its own? A failure
# here is the brew clang / Xcode 26.6 SDK pairing and nothing to
# do with this project's sources.
section "probe 1: bare stdio.h"
printf '#include <stdio.h>\n' > "${probe}/bare.cpp"
"$CXX" -std=c++26 -fsyntax-only -H "${probe}/bare.cpp" 2>&1 | tail -n 60
echo "probe 1 exit: ${PIPESTATUS[0]}"

# Probe 2: the include order headers/wrappers/sys.ixx uses, as a
# plain TU. <functional> pulls libc++ (and clang's stddef.h) in
# ahead of the SDK's stdio.h, which is the ordering that trips the
# guard. A failure only here is the GMF in sys.ixx.
section "probe 2: sys.ixx include order, textual"
cat > "${probe}/order.cpp" <<'EOF'
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
EOF
"$CXX" -std=c++26 -fsyntax-only -H "${probe}/order.cpp" 2>&1 | tail -n 60
echo "probe 2 exit: ${PIPESTATUS[0]}"

# Probe 3: the same includes in a real global module fragment. A
# failure only here means the module purview is what loses the
# typedef, not the include order itself.
section "probe 3: sys.ixx include order, global module fragment"
cat > "${probe}/order.cppm" <<'EOF'
module;
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
export module sdk.probe;
EOF
"$CXX" -std=c++26 -x c++-module --precompile \
  -o "${probe}/order.pcm" "${probe}/order.cppm" 2>&1 | tail -n 60
echo "probe 3 exit: ${PIPESTATUS[0]}"

section "_SIZE_T after each probe"
for f in bare.cpp order.cpp; do
  echo -n "${f}: "
  "$CXX" -std=c++26 -E -dM "${probe}/${f}" 2>/dev/null | grep -c '^#define _SIZE_T$'
done

# Probes 1-3 run on brew clang's own default sysroot, which its
# config file pins to the CommandLineTools SDK. CMake does not:
# CMAKE_OSX_SYSROOT comes from xcrun, so the real build compiles
# against the xcode-select'ed Xcode SDK instead. That is the SDK
# the reported error came from, so repeat everything under it.
section "brew clang config file (where the default sysroot is pinned)"
cfg="$("$CXX" -v 2>&1 | sed -n 's/^Configuration file: //p')"
echo "--- ${cfg:-<none>}"
[ -n "$cfg" ] && [ -f "$cfg" ] && cat "$cfg"
echo "--- CommandLineTools SDK version"
plutil -extract ProductVersion raw \
  /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/SDKSettings.plist 2>/dev/null \
  || echo "(unreadable)"

section "probes under the Xcode SDK that CMake actually uses"
xcode_sdk="$(xcrun --show-sdk-path)"
for f in bare.cpp order.cpp; do
  echo "--- ${f} with -isysroot ${xcode_sdk}"
  "$CXX" -std=c++26 -isysroot "$xcode_sdk" -fsyntax-only "${probe}/${f}" 2>&1 | head -n 30
  echo "exit: ${PIPESTATUS[0]}"
done
echo "--- order.cppm with -isysroot ${xcode_sdk}"
"$CXX" -std=c++26 -isysroot "$xcode_sdk" -x c++-module --precompile \
  -o "${probe}/order-xcode.pcm" "${probe}/order.cppm" 2>&1 | head -n 30
echo "exit: ${PIPESTATUS[0]}"

# Whichever of the two above fails, this says which header the
# guard came from: -H prints the include stack, and the last SDK
# header entered before the error is the one that set _SIZE_T
# without the typedef.
section "include stack under the Xcode SDK, to the first error"
"$CXX" -std=c++26 -isysroot "$xcode_sdk" -fsyntax-only -H -ferror-limit=1 \
  "${probe}/bare.cpp" 2>&1 | grep -E '_size_t|_stdio|stddef|error' | head -n 40

# The two SDK trees differ somewhere or both would build. Compare
# the headers on the path to the failure.
section "Xcode SDK vs CommandLineTools SDK headers"
clt_sdk="/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
for h in usr/include/sys/_types/_size_t.h usr/include/_bounds.h usr/include/_stdio.h usr/include/sys/cdefs.h; do
  echo "--- ${h}"
  diff "${clt_sdk}/${h}" "${xcode_sdk}/${h}" | head -n 25 || true
done

echo "::endgroup::"
exit 0
