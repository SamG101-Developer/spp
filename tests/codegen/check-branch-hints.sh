#!/usr/bin/env bash

# End-to-end check of std::hints::likely, std::hints::unlikely and
# std::hints::cold_path. Builds a scratch project at rel and asserts, in the
# optimised lto.ll:
#   - a "case" on likely(c) weights its branch 2000:1 and unlikely(c) 1:2000;
#   - the hint survives a short-circuit condition, a guard and a "loop" condition;
#   - cold_path() in a branch weights the branch into it 1:2000, past a merge
#     block and in a "case ... of" "else" arm too;
#   - cold_path() with no branch leading to it marks the function "cold";
#   - the same "case" with no hint has no weights (negative control);
#   - the program still takes the branches the conditions choose.
# The hint is lost unless its "llvm.expect" is in the branch's own block
# (LowerExpectIntrinsic removes the block's expects after its branch).
# Each function prints its own text: bodies differing only in their hints are
# identical once lowered, and MergeFunctions (rel) would fold them into one,
# keeping one copy's weights, as it does not compare them.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-branch-hints.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

dir="${WORK}/hints"
new_project "${dir}"
cat > "${dir}/src/main.spp" <<'EOF'
use std::llvm::no_inline

!no_inline
fun hint_likely(x: S32) -> S32 {
    case std::hints::likely(x > 10) {
        std::console::println("big (likely)")
        ret 1
    }
    ret 2
}

!no_inline
fun hint_unlikely(x: S32) -> S32 {
    case std::hints::unlikely(x > 10) {
        std::console::println("big (unlikely)")
        ret 1
    }
    ret 2
}

!no_inline
fun hint_none(x: S32) -> S32 {
    case x > 10 {
        std::console::println("big (no hint)")
        ret 1
    }
    ret 2
}

!no_inline
fun hint_and(a: S32, b: S32) -> S32 {
    case std::hints::unlikely(a > 10 and b > 10) {
        std::console::println("both")
        ret 1
    }
    ret 2
}

!no_inline
fun hint_guard(x: Opt[S32]) -> S32 {
    case x of {
        is Some[S32](val) and std::hints::unlikely(val > 100) {
            std::console::println("huge")
            ret 1
        }
        else { ret 2 }
    }
}

!no_inline
fun hint_loop(n: S32) -> S32 {
    let mut i = 0
    loop std::hints::likely(i < n) {
        std::console::println("step")
        i += 1
    }
    ret i
}

!no_inline
fun cold_plain(x: S32) -> S32 {
    case x > 10 {
        std::hints::cold_path()
        std::console::println("big (cold)")
        ret 1
    }
    ret 2
}

!no_inline
fun cold_after_merge(a: S32, b: S32) -> S32 {
    case a > 10 {
        case b > 10 { std::console::println("b") }
        std::hints::cold_path()
        std::console::println("a")
        ret 1
    }
    ret 2
}

!no_inline
fun cold_arm(x: Opt[S32]) -> S32 {
    case x of {
        is Some[S32](val) { ret val }
        else {
            std::hints::cold_path()
            std::console::println("none")
            ret 0
        }
    }
}

!no_inline
fun cold_whole(x: S32) -> S32 {
    std::hints::cold_path()
    std::console::println("whole")
    ret x
}

fun main() -> Void {
    let v = std::hints::black_box(500)
    let w = std::hints::black_box(4)
    let total = hint_likely(std::hints::black_box(20)) + hint_unlikely(std::hints::black_box(5))
        + hint_none(std::hints::black_box(20)) + hint_and(std::hints::black_box(20), std::hints::black_box(5))
        + hint_guard(Some[S32](val=v)) + hint_loop(std::hints::black_box(2))
        + cold_plain(std::hints::black_box(5)) + cold_after_merge(std::hints::black_box(5), std::hints::black_box(5))
        + cold_arm(Some[S32](val=w)) + cold_whole(std::hints::black_box(5))
    case total == 22 { std::console::println("ok") }
}
EOF

# The "branch_weights" node on "fn"'s conditional branches, one "a:b" per branch.
weights_of() {
    local fn="$1" ir="$2"
    awk -v fn="${fn}" '$0 ~ "^define .*::" fn "\\$" { on = 1 } on && /br i1 .*!prof !/ { print } on && /^}/ { on = 0 }' \
        "${ir}" | grep -o '!prof ![0-9]*' | sed 's/!prof //' | while read -r md; do
        grep "^${md} = " "${ir}" | grep -o 'i32 [0-9]*, i32 [0-9]*' | sed 's/i32 //g; s/, /:/'
    done
}

expect_weights() {
    local fn="$1" want="$2" ir="$3" got
    got="$(weights_of "${fn}" "${ir}" | tr '\n' ' ' | sed 's/ $//')"
    if [[ "${got}" =~ ${want} ]]; then
        ok "${fn}: branch weights ${got:-none}"
    else
        fail "${fn}: expected branch weights matching '${want}', got '${got:-none}'"
    fi
}

# Whether "fn"'s definition carries the "cold" attribute, through its attribute group.
expect_cold() {
    local fn="$1" ir="$2" group
    group="$(grep -E "^define .*::${fn}\\$" "${ir}" | grep -o '#[0-9]*' | tail -1)"
    if [[ -n "${group}" ]] && grep -E "^attributes ${group} = \\{" "${ir}" | grep -qw cold; then
        ok "${fn}: marked cold"
    else
        fail "${fn}: expected the \"cold\" attribute (group '${group:-none}')"
    fi
}

if build_project "hints" "${dir}" rel; then
    run_exe "${EXE}" "${dir}/run.log"
    if (( RC != 0 )) || ! grep -qx "ok" "${dir}/run.log"; then
        fail "hints: a hinted branch went the wrong way (exit ${RC})" "${dir}/run.log"
    else
        ok "hints: every hinted branch goes the way its condition says"
    fi

    ir="$(find "${dir}/out" -type f -path "*/rel/llvm/lto.ll" | head -1)"
    expect_weights hint_likely '^2000:1$' "${ir}"
    expect_weights hint_unlikely '^1:2000$' "${ir}"
    expect_weights hint_none '^$' "${ir}"
    expect_weights hint_and '^1:[0-9]+$' "${ir}"
    expect_weights hint_guard '^1:[0-9]+$' "${ir}"
    # Loop rotation rescales the loop's weights, so only their presence is checked: an unhinted loop has none.
    expect_weights hint_loop '[0-9]+:[0-9]+' "${ir}"
    expect_weights cold_plain '^1:2000$' "${ir}"
    # The inner "case" leads only to cold code, so only the outer branch is weighted.
    expect_weights cold_after_merge '^1:2000$' "${ir}"
    expect_weights cold_arm '^2000:1$' "${ir}"
    expect_cold cold_whole "${ir}"
    if grep -q 'spp.hint.cold_path' "${ir}"; then
        fail "cold_path: a marker call was left in the module"
    else
        ok "cold_path: every marker call was removed"
    fi
else
    failures=$((failures + 1))
fi

finish
