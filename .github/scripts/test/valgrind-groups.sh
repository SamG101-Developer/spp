#!/usr/bin/env bash
# Run chosen `!test` groups under valgrind (docs/memory-analysis-refactor.md, M9.6): the compiled S++ code's frames,
# environments and buffers, which the gtest suite never runs. The whole S++ suite is too slow under valgrind, so only
# the groups that make and drop them run here. `--group` builds the executable with that group alone, and `--runner`
# runs it under valgrind with the memory cap lifted.
#
# SPP_EXE overrides the compiler (the lane's verified `build/spp` by default), so this also runs locally.
set -euo pipefail

exe="${SPP_EXE:-${PWD}/build/spp}"
supp="${PWD}/.github/scripts/test/valgrind.supp"
runner="valgrind --quiet --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=9 --suppressions=${supp}"

project_groups=(regression noalias)
std_groups=(
  std/generator std/future std/iterator std/vector std/rc std/mem/raw_buf std/mem/ops std/case_patterns std/option
  std/string
)

# `spp` works in the current directory: the repository's own project holds the regression groups, and fetches std.
cd project
failed=()
for group in "${project_groups[@]}"; do
  echo "::group::${group}"
  "$exe" test --group "$group" --runner "$runner" || failed+=("$group")
  echo "::endgroup::"
done
for group in "${std_groups[@]}"; do
  echo "::group::${group}"
  "$exe" test --lib std --group "$group" --runner "$runner" || failed+=("$group")
  echo "::endgroup::"
done

if [ "${#failed[@]}" -ne 0 ]; then
  echo "valgrind reported errors in: ${failed[*]}" >&2
  exit 1
fi
