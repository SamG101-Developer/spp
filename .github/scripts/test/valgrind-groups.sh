#!/usr/bin/env bash
# Run chosen `!test` groups under valgrind (docs/memory-analysis-refactor.md, M9.6): the compiled S++ code's frames,
# environments and buffers, which the gtest suite never runs. The whole S++ suite is too slow under valgrind, so only
# the groups that make and drop them run here. `--group` builds the executable with that group alone, and `--runner`
# runs it under valgrind with the memory cap lifted. The compiler's own `project/` is not in the repository, so the
# groups come from the standard library, in a scratch project `spp init` writes.
#
# SPP_EXE overrides the compiler (the lane's verified `build/spp` by default), so this also runs locally.
set -euo pipefail
source .github/scripts/lib/scratch-project.sh

exe="${SPP_EXE:-${PWD}/build/spp}"
supp="${PWD}/.github/scripts/test/valgrind.supp"
runner="valgrind --quiet --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=9 --suppressions=${supp}"

std_groups=(
  std/generator std/future std/iterator std/vector std/rc std/mem/raw_buf std/mem/ops std/case_patterns std/option
  std/string std/regression std/noalias
)

scratch_project "$exe" valgrind
failed=()
for group in "${std_groups[@]}"; do
  echo "::group::${group}"
  "$exe" test --lib std --group "$group" --runner "$runner" || failed+=("$group")
  echo "::endgroup::"
done

if [ "${#failed[@]}" -ne 0 ]; then
  echo "valgrind reported errors in: ${failed[*]}" >&2
  exit 1
fi
