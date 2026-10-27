#!/usr/bin/env bash

# End-to-end check of what a "case" tests at run time ("PatternSolver"'s plan).
# Builds a scratch project at dev and asserts:
#   - patterns match what they should: a variant nested in a tuple, a refined
#     "Some[Some[T]]", a literal in a payload or a tuple, a comparison, an
#     array with a "..", an "is" under a "?" used as a method receiver
#     (generated through the call's copy of it, B5);
#   - the subject is evaluated once, however many literals test it (B1);
#   - a literal in a variant's payload is tested only once the tag matched: its
#     "eq" call is in a block the tag check branches to (B2).
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-case-patterns.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

dir="${WORK}/patterns"
new_project "${dir}"
cat > "${dir}/src/main.spp" <<'EOF'
fun tuple_variant(t: (Opt[S32], S32)) -> S32 {
    ret case t of {
        is (Some[S32](val), y) { val + y }
        else { 0 }
    }
}

fun refined(x: Opt[Opt[S32]]) -> S32 {
    ret case x of {
        is Some[Some[S32]](val=Some[S32](val)) { val }
        is Some[None](..) { 100 }
        is None() { 200 }
    }
}

fun payload_literal(x: Opt[S32]) -> S32 {
    ret case x of {
        is Some[S32](val=5) { 1 }
        is Some[S32](val) { 2 }
        is None() { 3 }
    }
}

fun tuple_payload_literal(t: (Opt[S32], S32)) -> S32 {
    ret case t of {
        is (Some[S32](val=7), 1) { 1 }
        is (Some[S32](val), 1) { 2 }
        else { 3 }
    }
}

fun compare(x: S32) -> S32 {
    ret case x of {
        == 3 { 1 }
        == 4 { 2 }
        else { 0 }
    }
}

fun array_literal(a: Arr[S32, 3_uz]) -> S32 {
    ret case a of {
        is [1, b, c] { b + c }
        is [p, .., 9] { p }
        else { 0 }
    }
}

fun pick(b: Bool) -> Opt[U64] {
    ret case b { Some(val=10_u64) } else { None }
}

fun chained_try(t: (S32, S32)) -> Opt[U64] {
    let y = pick(t is (5, ..))?.add_checked(1_u64)?
    ret Some(val=y)
}

fun make() -> (S32, S32) {
    std::console::println("make")
    ret (1, 2)
}

fun evaluated_once() -> S32 {
    ret case make() of {
        is (1, x) { x }
        else { 0 }
    }
}

fun check(got: S32, want: S32, what: &StrView) -> Void {
    case got != want {
        std::console::println(what)
        std::process::exit_checked(1)
    }
}

fun main() -> Void {
    let none: Opt[S32] = None
    let some3: Opt[S32] = Some[S32](val=3)
    check(tuple_variant((some3, 2)), 5, "tuple_variant some")
    check(tuple_variant((none, 2)), 0, "tuple_variant none")

    let inner5: Opt[S32] = Some[S32](val=5)
    let inner_none: Opt[S32] = None
    let o1: Opt[Opt[S32]] = Some[Opt[S32]](val=inner5)
    let o2: Opt[Opt[S32]] = Some[Opt[S32]](val=inner_none)
    let o3: Opt[Opt[S32]] = None
    check(refined(o1), 5, "refined some some")
    check(refined(o2), 100, "refined some none")
    check(refined(o3), 200, "refined none")

    let some5: Opt[S32] = Some[S32](val=5)
    let some6: Opt[S32] = Some[S32](val=6)
    let none2: Opt[S32] = None
    check(payload_literal(some5), 1, "payload_literal 5")
    check(payload_literal(some6), 2, "payload_literal 6")
    check(payload_literal(none2), 3, "payload_literal none")

    let some7: Opt[S32] = Some[S32](val=7)
    let some8: Opt[S32] = Some[S32](val=8)
    let none3: Opt[S32] = None
    check(tuple_payload_literal((some7, 1)), 1, "tuple_payload_literal 7")
    check(tuple_payload_literal((some8, 1)), 2, "tuple_payload_literal 8")
    check(tuple_payload_literal((none3, 1)), 3, "tuple_payload_literal none")

    check(compare(3), 1, "compare 3")
    check(compare(4), 2, "compare 4")
    check(compare(5), 0, "compare 5")

    check(array_literal([1, 2, 3]), 5, "array 123")
    check(array_literal([4, 2, 9]), 4, "array 429")
    check(array_literal([4, 2, 8]), 0, "array 428")

    case chained_try((5, 0)) of {
        is Some[U64](val) { check(case val == 11_u64 { 1 } else { 0 }, 1, "chained_try 5 value") }
        else { check(0, 1, "chained_try 5") }
    }
    case chained_try((6, 0)) of {
        is Some[U64](val) { check(0, 1, "chained_try 6") }
        else { }
    }

    check(evaluated_once(), 2, "evaluated_once")
    std::console::println("ok")
}
EOF

if build_project "patterns" "${dir}" dev; then
    run_exe "${EXE}" "${dir}/run.log"
    if (( RC != 0 )) || ! grep -qx "ok" "${dir}/run.log"; then
        fail "patterns: a case matched the wrong branch (exit ${RC})" "${dir}/run.log"
    else
        ok "patterns: every case matched the branch it should"
    fi

    makes="$(grep -cx "make" "${dir}/run.log")"
    if (( makes != 1 )); then
        fail "a literal test evaluated the subject again: \"make\" ran ${makes} times" "${dir}/run.log"
    else
        ok "a literal test reads the subject, evaluated once"
    fi

    # The block holding payload_literal's first "eq" call, and the blocks the
    # tag check ("case.pattern.is") branches to.
    ll="$(find "${dir}/out" -path "*/dev/llvm/main.ll" | head -1)"
    fn="$(awk '/^define .*payload_literal/,/^}/' "${ll}")"
    eq_block="$(awk '/^"?[^ ]+"?:/ { label = $1 }
        /call i1 @".*::eq/ { sub(/:$/, "", label); print label; exit }' <<< "${fn}")"
    if [[ -z "${eq_block}" ]]; then
        fail "payload_literal: no \"eq\" call found in its llvm"
    elif grep -F 'br i1 %"case.pattern.is' <<< "${fn}" | grep -qF "label %${eq_block},"; then
        ok "a literal in a payload is tested only once the tag matched"
    else
        fail "a literal in a payload is tested whatever the tag: block ${eq_block} is not reached from the tag check"
    fi
else
    failures=$((failures + 1))
fi

finish
