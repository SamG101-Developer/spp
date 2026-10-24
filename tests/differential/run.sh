#!/usr/bin/env bash
# The dev/rel differential suite: every program here is built
# at "dev" and at "rel", run, and what it prints and how it
# exits compared. A difference is a lowering or optimisation
# bug, found without anyone writing down what the program
# should print. A program that traps has to trap in both.
#
#   tests/differential/run.sh [path/to/spp] [name ...]
#
# Each program prints its results, so a difference shows where
# it is. Add the repro of every miscompile found here.

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
spp_arg="${1:-}"
shift || true
# shellcheck source=tests/codegen/lib.sh
source "${HERE}/../codegen/lib.sh" "${spp_arg}"

programs=()
if (( $# == 0 )); then
    programs=("${HERE}"/*.spp)
else
    for name in "$@"; do
        programs+=("${HERE}/${name%.spp}.spp")
    done
fi

for program in "${programs[@]}"; do
    name="$(basename "${program}" .spp)"
    if [[ ! -f "${program}" ]]; then
        fail "${name}: no such program"
        continue
    fi

    declare -A logs=() codes=()
    built=true
    for mode in dev rel; do
        dir="${WORK}/${name}-${mode}"
        new_project "${dir}"
        cp "${program}" "${dir}/src/main.spp"
        if ! build_project "${name} [${mode}]" "${dir}" "${mode}"; then
            failures=$((failures + 1))
            built=false
            break
        fi
        run_exe "${EXE}" "${dir}/run.log"
        # A trap's backtrace frames name the binary and differ with inlining.
        grep -v '^  #[0-9]' "${dir}/run.log" > "${dir}/compare.log"
        logs[${mode}]="${dir}/compare.log"
        codes[${mode}]="${RC}"
    done
    "${built}" || continue

    if [[ "${codes[dev]}" != "${codes[rel]}" ]]; then
        fail "${name}: exits ${codes[dev]} at dev but ${codes[rel]} at rel" "${logs[rel]}"
    elif ! cmp -s "${logs[dev]}" "${logs[rel]}"; then
        diff -u --label dev --label rel "${logs[dev]}" "${logs[rel]}" > "${WORK}/${name}.diff"
        fail "${name}: prints differently at dev and rel" "${WORK}/${name}.diff"
    else
        ok "${name}: $(wc -l < "${logs[dev]}") line(s) alike, exit ${codes[dev]} at both"
    fi
done

finish
