#!/usr/bin/env bash
# Run the standard library's unit tests built for a target, under
# what runs that target here: its qemu, or (musl) Alpine. Reuses
# the scratch project cross-codegen.sh made, so the hardening it
# switched off for the target is off for these too.
set -euo pipefail

TRIPLE="$1"
QEMU="${2:-}"
SYSROOT="${3:-}"
RUN_IN="${4:-}"

SPP="${PWD}/build/spp"
cd "${RUNNER_TEMP:-/tmp}/cross"

# The runner goes in front of the test executable; SPP_TEST_ONLY,
# which picks one test when a failure is re-run alone, has to reach
# the program through it (qemu passes the environment on).
if [ -n "$QEMU" ]; then
  RUNNER="qemu-${QEMU} -L ${SPP_QEMU_SYSROOT:-/usr/${SYSROOT}}"
elif [ "$RUN_IN" = alpine ]; then
  RUNNER="docker run --rm -e SPP_TEST_ONLY -v ${PWD}:${PWD} ${SPP_MUSL_RUN_IMAGE}"
else
  echo "::error::${TRIPLE} has nothing to run its tests under"
  exit 1
fi

echo "::group::spp test --all-libs --target ${TRIPLE}"
set +e +o pipefail
"$SPP" test --all-libs --target "$TRIPLE" --runner "$RUNNER" 2>&1 | tr '\r' '\n' | grep -v '^\s*$'
STATUS="${PIPESTATUS[0]}"
set -e -o pipefail
echo "::endgroup::"
if [ "$STATUS" -ne 0 ]; then
  echo "::error::the unit tests for ${TRIPLE} failed under ${RUNNER%% *} (exit ${STATUS})"
  exit 1
fi
echo "ok: ${TRIPLE} unit tests"
