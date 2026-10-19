#!/usr/bin/env bash

# End-to-end check that "[binary.codegen] jump_table_bounds" leaves no switch
# that can jump through an entry past the end of its jump table. On x86 that
# means no jump tables at all: switches are built as compares. Builds a scratch
# project whose dense "case ... of" over twelve values would otherwise become a
# jump table, and asserts, at dev and rel:
#   - the program runs and prints what it computed;
#   - with "jump_table_bounds = true" the program's own object has no indirect
#     jump that is not a type-checked call's tail (no jump table);
#   - with it false at rel the same switch is a jump table, so the key, and not
#     the shape of the program, is what removed it. Not at dev: a "case" is
#     emitted as a chain of compares, and only the optimiser folds that into a
#     switch, so there is no table to remove.
# The AArch64 half (a clamped index, "aarch64-jump-table-hardening") is not
# run here.
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
use std::llvm::no_inline

!no_inline
fun f0() -> U64 {
  ret 11_u64
}

!no_inline
fun f1() -> U64 {
  ret 48_u64
}

!no_inline
fun f2() -> U64 {
  ret 85_u64
}

!no_inline
fun f3() -> U64 {
  ret 21_u64
}

!no_inline
fun f4() -> U64 {
  ret 58_u64
}

!no_inline
fun f5() -> U64 {
  ret 95_u64
}

!no_inline
fun f6() -> U64 {
  ret 31_u64
}

!no_inline
fun f7() -> U64 {
  ret 68_u64
}

!no_inline
fun f8() -> U64 {
  ret 4_u64
}

!no_inline
fun f9() -> U64 {
  ret 41_u64
}

!no_inline
fun f10() -> U64 {
  ret 78_u64
}

!no_inline
fun f11() -> U64 {
  ret 14_u64
}

!no_inline
fun pick(n: U64) -> U64 {
  ret case n of {
    == 0_u64 { f0() }
    == 1_u64 { f1() }
    == 2_u64 { f2() }
    == 3_u64 { f3() }
    == 4_u64 { f4() }
    == 5_u64 { f5() }
    == 6_u64 { f6() }
    == 7_u64 { f7() }
    == 8_u64 { f8() }
    == 9_u64 { f9() }
    == 10_u64 { f10() }
    == 11_u64 { f11() }
    else { 0_u64 }
  }
}

fun main() -> Void {
  let mut sum = 0_u64
  loop i in std::range::Range[U64]::between(0_u64, 14_u64) {
    sum = sum + pick(i)
  }
  let s = Str::from(sum)
  std::console::println(&s)
  std::mem::ops::drop(s)
}
EOF
}

# One case: build, run, and count the indirect jumps that no type check
# guards, which on x86 are jump tables.
check() {
    local mode="$1" bounds="$2"
    local name="${mode}, jump_table_bounds=${bounds}"
    local dir="${WORK}/${mode}-${bounds}/switched"
    make_project "${dir}" "${bounds}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne 0 ]] || ! grep -qx "554" "${dir}/run.log"; then
        fail "${name}: expected \"554\" and exit 0 (exit ${RC})" "${dir}/run.log"
        return
    fi

    objdump -d --no-show-raw-insn "${OBJ}" > "${dir}/spp.dis"
    local tables
    tables="$(awk '/add +-0x4\(%r[a-z0-9]+\),%r10d/{chk=NR} /jmp +\*%/{if (NR-chk>3) n++} END{print n+0}' "${dir}/spp.dis")"

    if [[ "${bounds}" == true && ${tables} -eq 0 ]]; then
        ok "${name}: no jump tables"
    elif [[ "${bounds}" == false && ${mode} == rel && ${tables} -gt 0 ]]; then
        ok "${name}: ${tables} jump table(s)"
    elif [[ "${bounds}" == false && ${mode} == dev ]]; then
        ok "${name}: ${tables} jump table(s), as dev builds none"
    else
        fail "${name}: ${tables} unchecked indirect jump(s)"
    fi
}

for mode in dev rel; do
    check "${mode}" true
    check "${mode}" false
done

finish
