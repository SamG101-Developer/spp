#!/usr/bin/env bash

# End-to-end check of a spread of a tuple that is not a name (open-bug 52). Builds a scratch project at dev and
# asserts:
#   - "sum(..make())", "sum(&..make())" and "(0, ..make())" give the elements of the tuple "make" returns, and a
#     tuple of owned strings spread by move gives each string to its parameter;
#   - "make" runs once per spread, not once per element read off its tuple.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-spread-operand.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

dir="${WORK}/spread"
new_project "${dir}"
cat > "${dir}/src/main.spp" <<'EOF'
fun make() -> (S32, S32, S32) {
    std::console::println("make")
    ret (2, 3, 4)
}

fun strs() -> (Str, Str) {
    std::console::println("strs")
    ret (Str::from("a"), Str::from("b"))
}

fun sum(a: S32, b: S32, c: S32) -> S32 {
    ret a + b + c
}

fun sum_refs(a: &S32, b: &S32, c: &S32) -> S32 {
    ret a@ + b@ + c@
}

fun show(a: Str, b: Str) -> Void {
    std::console::println(&a)
    std::console::println(&b)
    std::mem::ops::drop(a)
    std::mem::ops::drop(b)
}

fun check(got: S32, want: S32, what: &StrView) -> Void {
    case got != want {
        std::console::println(what)
        std::process::exit_checked(1)
    }
}

fun main() -> Void {
    check(sum(..make()), 9, "call spread")
    check(sum_refs(&..make()), 9, "borrowed call spread")
    let t = (1, ..make())
    check(t.0 + t.1 + t.2 + t.3, 10, "tuple spread")
    show(..strs())
    std::console::println("ok")
}
EOF

if build_project "spread" "${dir}" dev; then
    run_exe "${EXE}" "${dir}/run.log"
    if (( RC != 0 )) || ! grep -qx "ok" "${dir}/run.log" || ! grep -qx "b" "${dir}/run.log"; then
        fail "spread: a spread of a call gave the wrong elements (exit ${RC})" "${dir}/run.log"
    else
        ok "spread: a spread of a call gives the elements of its tuple"
    fi

    makes="$(grep -cx "make" "${dir}/run.log")"
    strs="$(grep -cx "strs" "${dir}/run.log")"
    if (( makes != 3 || strs != 1 )); then
        fail "a spread evaluated its operand once per element: \"make\" ran ${makes} times for 3 spreads, \"strs\" ${strs} for 1" "${dir}/run.log"
    else
        ok "a spread evaluates its operand once"
    fi
else
    failures=$((failures + 1))
fi

finish
