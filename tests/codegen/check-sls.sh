#!/usr/bin/env bash

# Check that "[binary.codegen] sls" hardens straight-line speculation: every
# return and indirect jump the compiler emits is followed by a trap ("int3" on
# x86), so the bytes after one are never run speculatively. Builds a scratch
# project with returns, a dispatch through a closure and a "case" big enough
# for a jump table, and disassembles its object (libc's start-up code is not
# ours), asserting at dev and rel:
#   - with "sls = true" every "ret" and indirect "jmp" is followed by "int3";
#   - with "sls = false" none is, so the setting, and not the code shape, is
#     what put them there;
#   - the program still runs.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-sls.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"
require_x86_64

make_project() {
    local dir="$1" sls="$2"
    new_project "${dir}" "
[binary.codegen]
sls = ${sls}"

    cat > "${dir}/src/main.spp" <<'EOF'
use std::llvm::no_inline

!no_inline
fun pick(n: U64) -> U64 {
  ret case n of {
    == 0_u64 { 11_u64 }
    == 1_u64 { 23_u64 }
    == 2_u64 { 37_u64 }
    == 3_u64 { 41_u64 }
    == 4_u64 { 59_u64 }
    == 5_u64 { 67_u64 }
    else { 0_u64 }
  }
}

fun main() -> Void {
  let f = (x: U64) pick(x)
  let mut sum = f(3_u64)
  loop i in std::range::Range[U64]::between(0_u64, 7_u64) {
    sum = sum + pick(i)
  }
  let s = Str::from(sum)
  std::console::println(&s)
  std::mem::ops::drop(s)
}
EOF
}

# One case: build, run, and count the returns and indirect jumps in the
# program's own object that are, and are not, followed by a trap.
check() {
    local mode="$1" sls="$2"
    local name="${mode}, sls=${sls}"
    local dir="${WORK}/${mode}-${sls}/sls"
    make_project "${dir}" "${sls}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne 0 ]]; then
        fail "${name}: the program did not run cleanly (exit ${RC})" "${dir}/run.log"
        return
    fi

    # Each branch, and whether the instruction after it is a trap.
    local counts
    counts="$(objdump -d --no-show-raw-insn "${OBJ}" | awk '
        /^ *[0-9a-f]+:/ {
            insn = $2
            if (pending) { if (insn == "int3") { trapped++ } else { open++ } ; pending = 0 }
            if (insn == "ret" || (insn == "jmp" && $3 ~ /^\*/)) { pending = 1 }
        }
        /^$/ { if (pending) { open++ ; pending = 0 } }
        END { if (pending) { open++ } ; printf "%d %d", trapped + 0, open + 0 }')"
    local trapped="${counts% *}" open="${counts#* }"

    if [[ "${sls}" == "true" && ${trapped} -gt 0 && ${open} -eq 0 ]] ||
       [[ "${sls}" == "false" && ${trapped} -eq 0 && ${open} -gt 0 ]]; then
        ok "${name}: ${trapped} trapped, ${open} not"
    else
        fail "${name}: ${trapped} trapped, ${open} not"
    fi
}

for mode in dev rel; do
    check "${mode}" true
    check "${mode}" false
done

finish
