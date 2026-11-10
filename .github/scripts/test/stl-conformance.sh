#!/usr/bin/env bash
# Drive the compiler the way a person does: an empty directory,
# `spp init`, `spp build`, then the standard library's own tests
# through `spp test --all-libs`. The gtest suite proves the
# compiler's internals; only this exercises the STL end to end.
#
# `spp test` exits non-zero on a failed test and on a suite that
# matched none, so a clean run is the whole conformance check.
set -euo pipefail
source .github/scripts/lib/scratch-project.sh

exe="${PWD}/build/spp"
if [ "${RUNNER_OS:-Linux}" = "Windows" ]; then
  exe="${exe}.exe"
fi

scratch_project "$exe" conformance

# Keys the lane's target cannot have (the lane's "off" input).
features_off "${SPP_CI_OFF:-}"

"$exe" build
if [ "${RUNNER_OS:-Linux}" != "macOS" ]; then
  "$exe" test --all-libs
  exit
fi

# On macOS a test that takes its process down can do it with
# nothing on stderr, so the failures `spp test` lists are each
# run again under lldb, which prints every thread's stack where
# it stopped. The executable is named after the project folder.
log="${RUNNER_TEMP:-/tmp}/stl-conformance.log"
status=0
"$exe" test --all-libs 2>&1 | tee "$log" || status=$?
[ "$status" -eq 0 ] && exit 0

binary="$(find out -maxdepth 3 -type f -path '*/rel/*' -name conformance -perm -u+x | head -n 1)"
if [ -z "$binary" ]; then
  echo "::warning::no conformance executable under out/*/rel to run under lldb"
  exit "$status"
fi
tr -d '\r' < "$log" | sed -n '/^Failed:/,$p' | sed -n 's/^  \([A-Za-z_][A-Za-z0-9_:]*\)$/\1/p' | head -n 5 |
  while read -r name; do
    echo "::group::lldb: ${name}"
    SPP_TEST_ONLY="$name" xcrun lldb --batch -o run -k "thread backtrace all" -k quit -- "$binary" || true
    echo "::endgroup::"
  done
exit "$status"
