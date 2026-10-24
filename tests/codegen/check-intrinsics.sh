#!/usr/bin/env bash

# Every numeric std intrinsic, computed at compile time and at run time (P4.6).
# "intrinsics.py" writes the programs: each case computes an intrinsic as a
# "cmp" constant, at run time from values the optimiser cannot see, and in the
# generator from the run-time semantics, and reports which of the three
# disagree; one with no compile-time form is checked against the generator's
# value. Every intrinsic in the compiler's registry has cases, or a reason to be
# left out. Each program builds and runs at dev and at rel, where the optimiser
# folds the run-time side too. Known failures are listed in "intrinsics.py",
# and one that passes fails the check until its entry is removed.
#
#   [JOBS=4] [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-intrinsics.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
gen="${WORK}/gen"
python3 "${HERE}/intrinsics.py" emit "${ROOT}/sources/spp/codegen/builtins.cpp" "$(std_vcs)/std/src" "${gen}" || exit 1

# Build and run one program at one mode, leaving "<prog>.<mode>.status" (the
# exit code, or "build-failed") and "<prog>.<mode>.log" beside it.
run_one() {
    local prog="$1" mode="$2"
    local dir="${WORK}/${prog}-${mode}/intrinsics"
    new_project "${dir}"
    cp "${gen}/${prog}.spp" "${dir}/src/main.spp"
    if ! (cd "${dir}" && "${SPP}" build -m "${mode}" > "${dir}/build.log" 2>&1); then
        echo build-failed > "${gen}/${prog}.${mode}.status"
        cp "${dir}/build.log" "${gen}/${prog}.${mode}.log"
        return
    fi
    run_exe "$(find "${dir}/out" -type f -path "*/${mode}/intrinsics" | head -1)" "${gen}/${prog}.${mode}.log"
    echo "${RC}" > "${gen}/${prog}.${mode}.status"
}

jobs=0
for source in "${gen}"/*.spp; do
    prog="$(basename "${source}" .spp)"
    for mode in dev rel; do
        run_one "${prog}" "${mode}" &
        jobs=$((jobs + 1))
        if (( jobs >= ${JOBS:-4} )); then
            wait -n
            jobs=$((jobs - 1))
        fi
    done
done
wait

python3 "${HERE}/intrinsics.py" judge "${gen}" || failures=$((failures + 1))
finish
