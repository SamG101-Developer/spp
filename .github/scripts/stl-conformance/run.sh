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

"$exe" build
"$exe" test --all-libs
