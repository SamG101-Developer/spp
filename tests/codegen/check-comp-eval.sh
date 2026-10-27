#!/usr/bin/env bash

# End-to-end check that evaluating a "cmp" function at compile time leaves its body as written, at dev and rel. Each
# function is evaluated once through a "cmp" constant, then called at run time with another argument. Evaluating a
# tuple, array or repeated array literal used to write its folded elements back into the body, so the run-time call
# returned the compile-time call's values. The unit tests only build, so that is only seen by running it.
#
#   tests/codegen/check-comp-eval.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' PROGRAM <<'SPP'
cmp fun pair(x: S32) -> (S32, S32) { ret (x, x + 1) }
cmp fun both(x: S32) -> [S32; 2_uz] { ret [x, x] }
cmp fun many(x: S32) -> [S32; 2_uz] { ret [x; 2_uz] }

cmp p: (S32, S32) = pair(1)
cmp b: [S32; 2_uz] = both(1)
cmp m: [S32; 2_uz] = many(1)

fun show(v: S32) -> Void {
    let s = Str::from(v)
    std::console::println(&s)
    std::mem::ops::drop(s)
}

fun main() -> Void {
    let t = pair(5)
    show(t.0)
    show(t.1)
    let [a0, a1] = both(7)
    show(a0)
    show(a1)
    let [r0, r1] = many(9)
    show(r0)
    show(r1)
}
SPP

read -r -d '' WANT <<'OUT'
5
6
7
7
9
9
OUT

for mode in dev rel; do
    dir="${WORK}/comp-eval-${mode}/compeval"
    new_project "${dir}"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    if ! build_project "comp eval [${mode}]" "${dir}" "${mode}"; then
        failures=$((failures + 1))
        continue
    fi
    run_exe "${EXE}" "${dir}/run.log"
    diff -u --label want --label got <(printf '%s\n' "${WANT}") "${dir}/run.log" > "${dir}/run.diff"
    if (( RC != 0 )) || [[ -s "${dir}/run.diff" ]]; then
        fail "comp eval [${mode}]: the run-time calls returned the wrong values (exit ${RC})" "${dir}/run.diff"
    else
        ok "comp eval [${mode}]"
    fi
done

finish
