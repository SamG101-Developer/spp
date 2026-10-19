#!/usr/bin/env bash

# Check that "[binary.codegen] scrub" zeroes the registers a function used just
# before it returns. Builds a scratch project with a loop, a dispatch through a
# closure and a "case" big enough for a jump table, and disassembles its object
# (libc's start-up code is not ours), counting the self-"xor"s (a register
# zeroed) that run straight into each "ret". Asserts, at dev and rel:
#   - "scrub = true" zeroes more registers before returns than "scrub = false"
#     (which still has some: a function returning 0 ends "xor %eax,%eax"), so
#     the setting is what added them;
#   - the program still runs and prints what it computed.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-scrub.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"
require_x86_64

make_project() {
    local dir="$1" scrub="$2"
    new_project "${dir}" "
[binary.codegen]
scrub = ${scrub}"

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

# Count the registers zeroed immediately before every "ret" in the program's
# own object, after a build with "scrub" on or off. Echoes the count, or
# nothing when the build or the run went wrong (reported on stderr, since
# stdout is the count).
zeroed_before_ret() {
    local mode="$1" scrub="$2"
    local name="${mode}, scrub=${scrub}"
    local dir="${WORK}/${mode}-${scrub}/scrub"
    make_project "${dir}" "${scrub}"
    build_project "${name}" "${dir}" "${mode}" >&2 || return
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne 0 ]] || ! grep -qx 279 "${dir}/run.log"; then
        echo "FAIL  ${name}: the program did not print 279 (exit ${RC})" >&2
        sed 's/^/        /' "${dir}/run.log" | head -12 >&2
        return
    fi

    # A run of self-xors ending at a "ret" is the scrub sequence.
    objdump -d --no-show-raw-insn "${OBJ}" | awk '
        /^ *[0-9a-f]+:/ {
            insn = $2
            if (insn == "ret") { total += run ; run = 0 ; next }
            split($3, ops, ",")
            if (insn ~ /^xor/ && ops[1] == ops[2]) { run++ } else { run = 0 }
        }
        END { print total + 0 }'
}

check() {
    local mode="$1"
    local on off
    on="$(zeroed_before_ret "${mode}" true)"
    off="$(zeroed_before_ret "${mode}" false)"
    if [[ -z "${on}" || -z "${off}" ]]; then
        failures=$((failures + 1))
    elif (( on > off )); then
        ok "${mode}: ${on} registers zeroed before returns with scrub, ${off} without"
    else
        fail "${mode}: ${on} registers zeroed before returns with scrub, ${off} without"
    fi
}

for mode in dev rel; do
    check "${mode}"
done

finish
