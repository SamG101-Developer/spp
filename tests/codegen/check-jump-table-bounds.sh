#!/usr/bin/env bash

# End-to-end check that "[binary.codegen] jump_table_bounds" leaves no switch
# that can jump through an entry past the end of its jump table, even
# speculatively ("codegen::ApplyHardenedJumpTables"). LLVM builds no tables of
# its own; a dense switch becomes ours instead: the index masked into range with
# no branch ("cmp", "sbb", "and"), the default in slot 0, and one memory-indirect
# jump through the table. Builds a scratch project with a dense twelve-way
# "case ... of" whose arms do real work on a value the optimiser cannot see, and
# asserts, at dev and rel:
#   - the program prints the right sum for every value in range, both edges,
#     values just past the end, and the largest U64 (all the default);
#   - with "jump_table_bounds = true" the program's own object has none of LLVM's
#     jump tables (a 4-byte entry read, then "jmp *%reg"), and at rel at least
#     one of ours ("jmp *disp(%base,%idx,8)") after the masking "sbb";
#   - with it false at rel LLVM's own table is back, so the key, and not the shape
#     of the program, is what replaced it. Not at dev: a "case" is emitted as a
#     chain of compares, and only the optimiser folds that into a switch.
# x86 disassembly only; the lowering itself is plain IR and runs on every target.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-jump-table-bounds.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"
require_x86_64

make_project() {
    local dir="$1" bounds="$2"
    new_project "${dir}" "
[binary.codegen]
jump_table_bounds = ${bounds}"

    cat > "${dir}/src/main.spp" <<'EOF'
use std::hints::black_box
use std::llvm::no_inline

!no_inline
fun pick(n: U64, x: U64) -> U64 {
  ret case n of {
    == 0_u64 { x + 1_u64 }
    == 1_u64 { x * 3_u64 }
    == 2_u64 { x + 7_u64 }
    == 3_u64 { x * 5_u64 }
    == 4_u64 { x + 11_u64 }
    == 5_u64 { x * 7_u64 }
    == 6_u64 { x + 13_u64 }
    == 7_u64 { x * 9_u64 }
    == 8_u64 { x + 17_u64 }
    == 9_u64 { x * 11_u64 }
    == 10_u64 { x + 19_u64 }
    == 11_u64 { x * 13_u64 }
    else { 1000_u64 }
  }
}

fun main() -> Void {
  let mut sum = 0_u64
  loop i in std::range::Range[U64]::between(0_u64, 16_u64) {
    sum = sum + pick(black_box(i), black_box(i * 7_u64))
  }
  sum = sum + pick(black_box(1000000_u64), black_box(5_u64))
  sum = sum + pick(black_box(18446744073709551615_u64), black_box(5_u64))
  let s = Str::from(sum)
  std::console::println(&s)
  std::mem::ops::drop(s)
}
EOF
}

# One case: build, run, and count LLVM's jump tables and ours.
check() {
    local mode="$1" bounds="$2"
    local name="${mode}, jump_table_bounds=${bounds}"
    local dir="${WORK}/${mode}-${bounds}/switched"
    make_project "${dir}" "${bounds}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne 0 ]] || ! grep -qx "8784" "${dir}/run.log"; then
        fail "${name}: expected \"8784\" and exit 0 (exit ${RC})" "${dir}/run.log"
        return
    fi

    objdump -d --no-show-raw-insn "${OBJ}" > "${dir}/spp.dis"
    local llvm_tables ours
    llvm_tables="$(awk '/movslq +\(%r[a-z0-9]+,%r[a-z0-9]+,4\)/{t=NR} /jmp +\*%r/{if (NR-t<=3) n++} END{print n+0}' "${dir}/spp.dis")"
    ours="$(awk '/sbb +%r[a-z0-9]+,%r[a-z0-9]+/{m=NR} /jmp +\*(0x[0-9a-f]+)?\(%r[a-z0-9]+,%r[a-z0-9]+,8\)/{if (NR-m<=3) n++} END{print n+0}' "${dir}/spp.dis")"

    if [[ "${bounds}" == true && ${llvm_tables} -eq 0 && ( ${mode} == dev || ${ours} -gt 0 ) ]]; then
        ok "${name}: no LLVM jump tables, ${ours} masked table jump(s)"
    elif [[ "${bounds}" == false && ${mode} == rel && ${llvm_tables} -gt 0 ]]; then
        ok "${name}: ${llvm_tables} LLVM jump table(s)"
    elif [[ "${bounds}" == false && ${mode} == dev ]]; then
        ok "${name}: ${llvm_tables} LLVM jump table(s), as dev builds none"
    else
        fail "${name}: ${llvm_tables} LLVM jump table(s), ${ours} masked table jump(s)"
    fi
}

for mode in dev rel; do
    check "${mode}" true
    check "${mode}" false
done

finish
