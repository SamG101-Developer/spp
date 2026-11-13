#!/usr/bin/env bash
set -uo pipefail

# Run the codegen checks: every tests/codegen/check-*.sh, or only those named
# (with or without the "check-" and ".sh"). Each builds and runs its own
# scratch projects, so they are run one after another, not in parallel.
#
# A check runs on the platforms its "# platforms:" line names (linux, macos,
# windows, for Git Bash); one without the line is Linux's, as most read ELF,
# Linux's kernel or its disassembly. The rest are listed as skipped here.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/run.sh [path/to/spp] [name ...]

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
SPP="$(realpath "${1:-${ROOT}/cmake-build-release-wsl-dev/spp}")"
readonly SPP
shift || true

case "$(uname -s)" in
    Linux) PLATFORM=linux ;;
    Darwin) PLATFORM=macos ;;
    MINGW* | MSYS* | CYGWIN*) PLATFORM=windows ;;
    *) PLATFORM="$(uname -s)" ;;
esac
readonly PLATFORM

# The platforms a check runs on: its "# platforms:" line, or Linux alone.
platforms_of() {
    local line
    line="$(grep -m1 -E '^# platforms:' "$1")"
    if [[ -n "${line}" ]]; then echo "${line#\# platforms:}"; else echo " linux"; fi
}

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
skipped=()
for check in "${checks[@]}"; do
    name="$(basename "${check}" .sh)"
    if [[ ! -f "${check}" ]]; then
        echo "no check ${name}"
        failed+=("${name}")
        continue
    fi
    platforms="$(platforms_of "${check}")"
    if [[ " ${platforms} " != *" ${PLATFORM} "* ]]; then
        skipped+=("${name}")
        continue
    fi
    echo "::group::${name}"
    bash "${check}" "${SPP}" || failed+=("${name}")
    echo "::endgroup::"
done

ran=$(( ${#checks[@]} - ${#skipped[@]} ))
(( ${#skipped[@]} > 0 )) && echo "skipped on ${PLATFORM} (${#skipped[@]}): ${skipped[*]}"
if (( ${#failed[@]} > 0 )); then
    echo "failed: ${failed[*]}"
    exit 1
fi
echo "all ${ran} checks passed on ${PLATFORM}"
