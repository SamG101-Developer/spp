#!/usr/bin/env bash

# End-to-end check of a binary fold over an operand that is not a name.
# Builds a scratch project at dev and asserts:
#   - ".. + make()", "make() * .." and "id(make()).." give the fold of the tuple
#     "make" returns;
#   - "make" runs once per fold, not once per element read off its tuple;
#   - a call fold's receiver and its unfolded arguments ("make().m(xs, arg())..")
#     run once per fold, not once per element (open-bugs.md #59).
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-fold-operand.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

dir="${WORK}/fold"
new_project "${dir}"
cat > "${dir}/src/main.spp" <<'EOF'
fun make() -> (S32, S32, S32) {
    std::console::println("make")
    ret (2, 3, 4)
}

fun id(a: S32) -> S32 {
    ret a
}

fun check(got: S32, want: S32, what: &StrView) -> Void {
    case got != want {
        std::console::println(what)
        std::process::exit_checked(1)
    }
}

fun main() -> Void {
    let sum = .. + make()
    let product = make() * ..
    check(sum, 9, "lhs fold")
    check(product, 24, "rhs fold")
    let ids = id(make())..
    check(ids.0 + ids.1 + ids.2, 9, "call fold")
    std::console::println("ok")
}
EOF

if build_project "fold" "${dir}" dev; then
    run_exe "${EXE}" "${dir}/run.log"
    if (( RC != 0 )) || ! grep -qx "ok" "${dir}/run.log"; then
        fail "fold: a fold over a call gave the wrong value (exit ${RC})" "${dir}/run.log"
    else
        ok "fold: a fold over a call gives the fold of its tuple"
    fi

    makes="$(grep -cx "make" "${dir}/run.log")"
    if (( makes != 3 )); then
        fail "a fold evaluated its operand once per element: \"make\" ran ${makes} times for 3 folds" "${dir}/run.log"
    else
        ok "a fold evaluates its operand once"
    fi
else
    failures=$((failures + 1))
fi

# A call fold copies its receiver and every argument it does not fold into each element's call, so a receiver or an
# argument that is a call ran once per element (open-bugs.md #59, fixed).
dir="${WORK}/fold-receiver"
new_project "${dir}"
cat > "${dir}/src/main.spp" <<'EOF'
cls C { }

sup C ext Copy { }

sup C {
    !public
    fun m(&self, x: S32, y: S32) -> Void { }
}

fun make() -> C {
    std::console::println("make")
    ret C()
}

fun arg() -> S32 {
    std::console::println("arg")
    ret 0
}

fun g(..xs: S32) -> Void {
    make().m(xs, arg())..
}

fun main() -> Void {
    g(1, 2, 3)
}
EOF

if build_project "fold-receiver" "${dir}" dev; then
    run_exe "${EXE}" "${dir}/run.log"
    makes="$(grep -cx "make" "${dir}/run.log")"
    args="$(grep -cx "arg" "${dir}/run.log")"
    if (( RC != 0 || makes != 1 || args != 1 )); then
        fail "a call fold ran its receiver ${makes} times and its unfolded argument ${args} times over 3 elements (exit ${RC})" "${dir}/run.log"
    else
        ok "a call fold evaluates its receiver and unfolded arguments once"
    fi
else
    failures=$((failures + 1))
fi

finish
