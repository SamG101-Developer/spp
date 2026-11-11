#!/usr/bin/env bash

# End-to-end check that a function's pack is its elements, not a tuple (pack-design.md): an instance has one
# parameter per element, a borrowed pack's elements are pointers, and nothing is built at the call. At dev and rel the
# program runs and prints what each element held, and folds over packs (binary, and a call per element whose results
# are folded in turn) give what folding the elements one by one does. A template's comp pack ("cmp ..ns: S32") folds,
# spreads and is collected into a tuple ("(..ns)") as a function pack does (next-steps.md N14.1).
#
#   tests/codegen/check-packs.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' PROGRAM <<'SPP'
fun show_one(s: &Str) -> Void {
    std::console::println(s)
}

fun refs(..xs: &Str) -> Void {
    show_one(xs.0)
    show_one(xs.1)
}

fun owned(..xs: S32) -> S32 {
    ret xs.0 + xs.1
}

fun total(..xs: S32) -> S32 {
    ret .. + xs
}

fun twice(t: S32) -> S32 {
    ret t + t
}

fun doubled_total(..xs: S32) -> S32 {
    let r = twice(xs)..
    ret .. + r
}

fun one[T](t: T) -> S32 {
    std::mem::ops::drop(t)
    ret 1
}

fun count[..Ts](..xs: Ts) -> S32 {
    let r = one(xs)..
    ret .. + r
}

fun comp_total[cmp ..ns: S32]() -> S32 {
    ret .. + ns
}

fun comp_doubled_total[cmp ..ns: S32]() -> S32 {
    let r = twice(ns)..
    ret .. + r
}

fun comp_spread[cmp ..ns: S32]() -> S32 {
    ret total(..ns)
}

fun comp_tuple_second[cmp ..ns: S32]() -> S32 {
    let t = (..ns)
    ret t.1
}

fun show(n: S32) -> Void {
    let s = Str::from(n)
    std::console::println(&s)
    std::mem::ops::drop(s)
}

fun main() -> Void {
    let a = Str::from("first")
    let b = Str::from("second")
    refs(&a, &b)
    let n = Str::from(owned(40_s32, 2_s32))
    std::console::println(&n)
    std::mem::ops::drop(a)
    std::mem::ops::drop(b)
    std::mem::ops::drop(n)
    show(total(1_s32, 2_s32, 3_s32))
    show(doubled_total(1_s32, 2_s32, 3_s32))
    show(count(1_s32, true, Str::from("a"), 2_u8))
    show(comp_total[1_s32, 2_s32, 3_s32]())
    show(comp_doubled_total[1_s32, 2_s32]())
    show(comp_spread[4_s32, 5_s32]())
    show(comp_tuple_second[7_s32, 8_s32]())
}
SPP

read -r -d '' WANT <<'OUT'
first
second
42
6
12
4
6
6
9
8
OUT

# The parameter list of the one instance whose mangled name holds "name": what follows the quoted name, to the
# closing parenthesis before the attributes.
params_of() {
    grep -E "^define .*${1}" "$2" | head -1 | sed -E 's/.*"\(//; s/\) #[0-9]+ \{$//'
}

for mode in dev rel; do
    dir="${WORK}/packs-${mode}/packs"
    new_project "${dir}"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    if ! build_project "packs [${mode}]" "${dir}" "${mode}"; then
        failures=$((failures + 1))
        continue
    fi
    run_exe "${EXE}" "${dir}/run.log"
    diff -u --label want --label got <(printf '%s\n' "${WANT}") "${dir}/run.log" > "${dir}/run.diff"
    same=$?
    if (( RC != 0 || same != 0 )); then
        fail "packs [${mode}]: the elements read back wrong (exit ${RC})" "${dir}/run.diff"
        continue
    fi
    [[ "${mode}" == rel ]] && { ok "packs [${mode}]"; continue; }

    ll="$(dirname "${OBJ}")/main.ll"
    refs="$(params_of 'refs' "${ll}")"
    owned="$(params_of 'owned' "${ll}")"
    if ! [[ "${refs}" =~ ^ptr\ [^,]*%0,\ ptr\ [^,]*%1$ ]]; then
        fail "packs [${mode}]: the borrowed pack is not two pointers: (${refs})"
    elif ! [[ "${owned}" =~ ^i32\ [^,]*%0,\ i32\ [^,]*%1$ ]]; then
        fail "packs [${mode}]: the owned pack is not two i32s: (${owned})"
    else
        ok "packs [${mode}]"
    fi
done
finish
