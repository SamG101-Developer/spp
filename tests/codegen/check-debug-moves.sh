#!/usr/bin/env bash

# End-to-end check that full debug info ends a binding's location where it is moved, and resumes it where it is given
# a value ("EndVariable", "ResumeVariable", llvm_debug): a debugger shows a moved-from binding as not available, not
# the bytes it no longer owns. Builds a scratch project at dev for a debugger ("--for-debugger": under the split stack,
# SafeStack rewrites these locations onto a register, upstream-watch U29), and asserts with gdb that:
#   - a binding shows its value once its "let" has given it one, and not before;
#   - after it is moved into a call, it is "<optimized out>", and after an assignment, it shows the new value;
#   - one moved on every branch of a "case" is "<optimized out>" after it;
#   - a parameter moved inside its function is "<optimized out>" after the move.
# Skipped without gdb.
#
#   tests/codegen/check-debug-moves.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

if ! command -v gdb > /dev/null; then
    echo "skipped: no gdb"
    exit 0
fi

read -r -d '' PROGRAM <<'SPP'
use std::hints::black_box
use std::llvm::no_inline

!no_inline
fun take(s: Str) -> U64 {
  let n = s.len()
  std::mem::ops::drop(s)
  ret n
}

!no_inline
fun look(flag: Bool) -> Str {
  let mut name = Str::from("first")
  let count = 3_u64
  let n = take(name)
  name = Str::from("second")
  let mut nums = Vec[S32]::new()
  nums.append(1)
  case flag {
    std::mem::ops::drop(nums)
  }
  else {
    std::mem::ops::drop(nums)
  }
  let total = n + count
  ret name
}

fun main() -> Void {
  let s = look(black_box(true))
  std::console::println(&s)
  std::mem::ops::drop(s)
}
SPP

# What gdb shows of "names", one per line, stopped on "line" of "exe" run from "dir"; raw (no formatters), so a
# "Str" reads as its fields and a moved one as "<optimized out>".
gdb_vars() {
    local dir="$1" exe="$2" line="$3"
    shift 3
    local prints=()
    local name
    for name in "$@"; do prints+=(-ex "print ${name}"); done
    (cd "${dir}" && gdb -batch -nx -ex "break src/main.spp:${line}" -ex run "${prints[@]}" "${exe}" 2>&1) \
        | grep -E '^\$[0-9]+ = ' | sed -E 's/^\$[0-9]+ = //'
}

# Whether a printed value is a live one or "<optimized out>".
state_of() {
    if [[ "$1" == "<optimized out>" ]]; then echo gone; else echo live; fi
}

# Line, then the expected state of "name" and "nums" there.
readonly CASES=(
    "14 live gone"
    "16 gone gone"
    "17 live gone"
    "19 live live"
    "25 live gone"
)

dir="${WORK}/moves"
new_project "${dir}"
printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
if ! build_project "dev" "${dir}" dev --for-debugger; then
    failures=$((failures + 1))
    finish
fi

for entry in "${CASES[@]}"; do
    read -r line want_name want_nums <<< "${entry}"
    mapfile -t values < <(gdb_vars "${dir}" "${EXE}" "${line}" name nums)
    got_name="$(state_of "${values[0]:-}")" got_nums="$(state_of "${values[1]:-}")"
    if [[ ${#values[@]} -ne 2 ]]; then
        fail "line ${line}: gdb printed ${#values[@]} values, not 2"
    elif [[ "${got_name} ${got_nums}" != "${want_name} ${want_nums}" ]]; then
        fail "line ${line}: name ${got_name}, nums ${got_nums}; expected name ${want_name}, nums ${want_nums}"
    else
        ok "line ${line}: name ${got_name}, nums ${got_nums}"
    fi
done

mapfile -t values < <(gdb_vars "${dir}" "${EXE}" 6 s)
mapfile -t after < <(gdb_vars "${dir}" "${EXE}" 8 s)
if [[ "$(state_of "${values[0]:-<optimized out>}")" != live || "$(state_of "${after[0]:-}")" != gone ]]; then
    fail "parameter: s is \"${values[0]:-}\" before its move and \"${after[0]:-}\" after"
else
    ok "parameter: s live before its move, gone after"
fi

finish
