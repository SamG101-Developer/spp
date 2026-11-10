#!/usr/bin/env bash
# STL conformance (stl-conformance.sh) under valgrind: every library's `!test` suite in one executable - the compiled
# S++ code's frames, environments and buffers, which the gtest suite never runs. A valgrind error fails the whole run,
# so `spp test` then re-runs each test alone under valgrind, which names the one at fault.
#
# Valgrind translates the program into executable memory it maps inside the program's own process, so W^X (MDWE)
# refuses its later mappings, and it does not know mseal. Both are off here.
#
# SPP_CI_EXE overrides the compiler (the lane's verified `build/spp` by default), so this also runs locally.
set -euo pipefail
source .github/scripts/lib/scratch-project.sh

exe="${SPP_CI_EXE:-${PWD}/build/spp}"
supp="${PWD}/.github/scripts/test/valgrind.supp"
runner="valgrind --quiet --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=9 --suppressions=${supp}"

scratch_project "$exe" valgrind_conformance
features_off "memory.map.deny_wx memory.map.seal"
"$exe" test --all-libs --runner "$runner"
