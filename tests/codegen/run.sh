#!/usr/bin/env bash
set -uo pipefail

# Run the codegen checks: every tests/codegen/check-*.sh, or only those named
# (with or without the "check-" and ".sh"). Each builds and runs its own
# scratch projects, so they are run one after another, not in parallel.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/run.sh [path/to/spp] [name ...]

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
SPP="$(realpath "${1:-${ROOT}/cmake-build-release-wsl-dev/spp}")"
readonly SPP
shift || true

checks=()
if (( $# == 0 )); then
    checks=("${HERE}"/check-*.sh)
else
    for name in "$@"; do
        name="${name#check-}"
        checks+=("${HERE}/check-${name%.sh}.sh")
    done
fi

failed=()
for check in "${checks[@]}"; do
    name="$(basename "${check}" .sh)"
    if [[ ! -f "${check}" ]]; then
        echo "no check ${name}"
        failed+=("${name}")
        continue
    fi
    echo "::group::${name}"
    bash "${check}" "${SPP}" || failed+=("${name}")
    echo "::endgroup::"
done

if (( ${#failed[@]} > 0 )); then
    echo "failed: ${failed[*]}"
    exit 1
fi
echo "all ${#checks[@]} checks passed"
