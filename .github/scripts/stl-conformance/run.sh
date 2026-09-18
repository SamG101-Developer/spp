#!/usr/bin/env bash
# Drive the compiler the way a person does: an empty directory,
# `spp init`, `spp build`, then the standard library's own tests
# through `spp test --all-libs`. The gtest suite proves the
# compiler's internals; only this exercises the STL end to end.
#
# `spp test` exits non-zero on a failed test and on a suite that
# matched none, so a clean run is the whole conformance check.
set -euo pipefail

exe="${PWD}/build/spp"
if [ "${RUNNER_OS:-Linux}" = "Windows" ]; then
  exe="${exe}.exe"
fi

# `spp init` names the project after its directory, which becomes a
# module folder under src, so the name is fixed rather than mktemp's.
project="${RUNNER_TEMP:-/tmp}/conformance"
rm -rf "$project"
mkdir -p "$project"
cd "$project"

"$exe" init

# `init` reports a directory it will not touch on stdout and still
# exits 0, so the structure is what says whether it ran.
if [ ! -f spp.toml ] || [ ! -f src/main.spp ]; then
  echo "::error::spp init wrote no project into ${project}"
  exit 1
fi

# The [vcs] section `init` writes points at the STL, so this also
# clones it.
"$exe" build
"$exe" test --all-libs
