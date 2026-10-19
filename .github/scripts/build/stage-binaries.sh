#!/usr/bin/env bash
# Stage exactly the binaries the test jobs run; publish the artifact name and each sha256 as step outputs.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

# These land in GITHUB_OUTPUT, so they are validated rather than quoted.
[[ "$STAGE_OS" =~ ^[a-z0-9.-]+$ ]] || { echo "::error::bad os '${STAGE_OS}'"; exit 1; }
[[ "$STAGE_COMPILER" =~ ^[a-z-]+$ ]] || { echo "::error::bad compiler '${STAGE_COMPILER}'"; exit 1; }
[[ "$STAGE_VARIANT" =~ ^[a-z0-9-]+$ ]] || { echo "::error::bad variant '${STAGE_VARIANT}'"; exit 1; }

exe=""
[ "$RUNNER_OS" = "Windows" ] && exe=".exe"
out="staged-bin"

# Tests go when this configure enabled them, not when a restored tree happens to hold the file.
with_tests=false
if grep -q '^SPP_BUILD_TESTS:BOOL=ON$' build/CMakeCache.txt; then
  with_tests=true
fi

# An explicit list, so nothing else from the build tree or the checkout rides along.
rm -rf "$out"
mkdir -p "$out"
cp "build/spp${exe}" "${out}/spp${exe}"
spp="$(sha256_of "${out}/spp${exe}")"
echo "spp${exe}             ${spp}"

{
  echo "artifact=bin-${STAGE_OS}-${STAGE_COMPILER}-${STAGE_VARIANT}"
  echo "spp-sha256=${spp}"
} >> "$GITHUB_OUTPUT"

if [ "$with_tests" = true ]; then
  mkdir -p "${out}/tests"
  cp "build/tests/spp_tests${exe}" "${out}/tests/spp_tests${exe}"
  tests="$(sha256_of "${out}/tests/spp_tests${exe}")"
  echo "tests/spp_tests${exe} ${tests}"
  echo "tests-sha256=${tests}" >> "$GITHUB_OUTPUT"
fi

# The record of what every later job in the run was checked against.
{
  echo "### Staged \`bin-${STAGE_OS}-${STAGE_COMPILER}-${STAGE_VARIANT}\`"
  echo
  echo "| file | sha256 |"
  echo "| --- | --- |"
  echo "| \`spp${exe}\` | \`${spp}\` |"
  if [ "$with_tests" = true ]; then
    echo "| \`tests/spp_tests${exe}\` | \`${tests}\` |"
  fi
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"
