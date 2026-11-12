#!/usr/bin/env bash

# End-to-end check that full debug info places each block's variables and lines in the block's own lexical block
# ("EnterDebugBlock", "LeaveDebugBlock", llvm_debug). Builds a scratch project at dev for a debugger ("--for-debugger"),
# whose function shadows a variable inside a "case" branch, and asserts that:
#   - inside the branch, the shadowing variable is the one a name reads ("print x" is 100, not the outer 8);
#   - after the branch, its variables are gone, and the outer one shows again;
#   - no compiler temporary ("$temp...") is shown as a variable;
#   - stepping through the function never stops in the branch not taken: the code after a "case" (storing its value)
#     is at the statement's line, not the last branch generated.
# The variables are read with gdb and the stepping with lldb (gdb also stops a second time on a line whose inlined
# checked add it steps over); each part is skipped where its debugger is not installed, or fails under CI's
# SPP_CI_REQUIRE_TOOLS.
#
#   tests/codegen/check-debug-scopes.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

LLDB="$(command -v lldb-23 || command -v lldb || echo lldb-23)"
readonly LLDB

# The branch taken holds lines 9 to 11; the one not taken, line 14.
read -r -d '' PROGRAM <<'SPP'
use std::hints::black_box
use std::llvm::no_inline

!no_inline
fun look(scale: U64) -> U64 {
  let x = scale + 1_u64
  let outer = 10_u64
  let y = case x > 0_u64 {
    let x = 100_u64
    let inner = x + outer
    inner
  }
  else {
    0_u64
  }
  let after = x + y
  ret after
}

fun main() -> Void {
  let r = look(black_box(7_u64))
  let s = Str::from(r)
  std::console::println(&s)
  std::mem::ops::drop(s)
}
SPP

# gdb stopped on "line" of "exe", run from the project directory "dir", printing the locals and "x".
gdb_at() {
    local dir="$1" exe="$2" line="$3"
    (cd "${dir}" && gdb -batch -nx -ex "break src/main.spp:${line}" -ex run -ex 'info locals' -ex 'print x' \
        "${exe}" 2>&1)
}

dir="${WORK}/scopes"
new_project "${dir}"
printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
if ! build_project "dev" "${dir}" dev --for-debugger; then
    failures=$((failures + 1))
    finish
fi

if have_tool gdb "gdb"; then
    gdb_at "${dir}" "${EXE}" 11 > "${dir}/inside.log"
    # shellcheck disable=SC2016 # gdb's output, not a shell variable
    if ! grep -qE '^\$1 = 100$' "${dir}/inside.log"; then
        fail "inside the branch: \"x\" is not the shadowing 100" "${dir}/inside.log"
    else
        ok "inside the branch: \"x\" is the shadowing one"
    fi

    gdb_at "${dir}" "${EXE}" 16 > "${dir}/after.log"
    # shellcheck disable=SC2016 # gdb's output, not a shell variable
    if grep -qE '^(inner|x = 100)' "${dir}/after.log"; then
        fail "after the branch: its variables still show" "${dir}/after.log"
    elif ! grep -qE '^\$1 = 8$' "${dir}/after.log"; then
        fail "after the branch: \"x\" is not the outer 8" "${dir}/after.log"
    elif grep -qF '$temp' "${dir}/after.log"; then
        fail "a compiler temporary shows as a variable" "${dir}/after.log"
    else
        ok "after the branch: its variables gone, the outer \"x\" back"
    fi
fi

if have_tool "${LLDB}" "lldb"; then
    nexts=()
    for _ in $(seq 1 11); do nexts+=(-o next); done
    (cd "${dir}" && "${LLDB}" -b -o "settings set target.source-map . ${dir}" -o 'breakpoint set -n main::look' \
        -o run "${nexts[@]}" "${EXE}" > "${dir}/steps.log" 2>&1)
    steps="$(grep -oE 'at main\.spp:[0-9]+' "${dir}/steps.log" | cut -d: -f2 | tr '\n' ' ')"
    if [[ " ${steps}" != *" 11 "* || " ${steps}" != *" 17 "* ]]; then
        fail "stepping: did not run through the branch to the return (lines ${steps})" "${dir}/steps.log"
    elif [[ " ${steps}" == *" 14 "* ]]; then
        fail "stepping: stopped in the branch not taken (lines ${steps})" "${dir}/steps.log"
    else
        ok "stepping: lines ${steps% }"
    fi
fi

finish
