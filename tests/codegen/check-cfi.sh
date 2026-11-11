#!/usr/bin/env bash

# End-to-end check that "[binary.codegen] cfi" types every indirect call
# (KCFI). Builds a scratch project that calls closures (with and without
# captures), a function passed as a value, and a generator it loops over,
# and asserts, at dev and rel:
#   - the program runs and prints what it computed;
#   - with "cfi = true" every indirect call in the program's own object is
#     preceded by the type check, and functions carry a type preamble;
#   - with "cfi = false" there are neither;
#   - a call through a function value whose code pointer C overwrote with a
#     function of another type is, with "cfi = true", reported as a failed
#     check through the one abort path (S10.5): exit 134, the check named, not
#     the signal its trap raised. With "cfi = false" nothing reports one.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-cfi.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"
require_x86_64

make_project() {
    local dir="$1" cfi="$2"
    new_project "${dir}" "
[binary.codegen]
cfi = ${cfi}
bti = ${cfi}"

    cat > "${dir}/src/main.spp" <<'EOF'
use std::llvm::no_inline

!no_inline
fun twice(f: FunRef[(U64,), U64], x: U64) -> U64 {
  ret f(f(x))
}

!no_inline
fun call(f: FunMov[(), U64]) -> U64 {
  ret f()
}

fun add_three(x: U64) -> U64 {
  ret x + 3_u64
}

cor counter(n: U64) -> std::iterator::Iterator[U64] {
  loop i in std::range::Range[U64]::between(0_u64, n) {
    gen i
  }
}

fun main() -> Void {
  let k = 5_u64
  let mut sum = twice((x: U64) -> U64 { ret x + 5_u64 }, 1_u64)
  sum = sum + twice(add_three, 1_u64)
  sum = sum + call((caps k) -> U64 { ret k })
  loop i in counter(4_u64) {
    sum = sum + i
  }
  let s = Str::from(sum)
  std::console::println(&s)
  std::mem::ops::drop(s)
}
EOF
}

# One case: build, run, and count indirect calls and their checks.
check() {
    local mode="$1" cfi="$2"
    local name="${mode}, cfi=${cfi}"
    local dir="${WORK}/${mode}-${cfi}/typed"
    make_project "${dir}" "${cfi}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne 0 ]] || ! grep -qx "29" "${dir}/run.log"; then
        fail "${name}: expected \"29\" and exit 0 (exit ${RC})" "${dir}/run.log"
        return
    fi

    objdump -d --no-show-raw-insn "${OBJ}" > "${dir}/spp.dis"
    local counts
    counts="$(awk '/add +-0x4\(%r[a-z0-9]+\),%r10d/{chk=NR} /call +\*%/{calls++; if (NR-chk<=3) ok++}
        END{print calls+0, ok+0}' "${dir}/spp.dis")"
    local calls="${counts% *}" checked="${counts#* }"
    local typed
    typed="$(nm "${OBJ}" | grep -c ' __cfi_')"

    if [[ "${cfi}" == true ]]; then
        if (( calls > 0 && checked == calls && typed > 0 )); then
            ok "${name}: ${checked}/${calls} indirect calls checked, ${typed} typed functions"
        else
            fail "${name}: ${checked}/${calls} indirect calls checked, ${typed} typed functions"
        fi
    elif (( checked == 0 && typed == 0 )); then
        ok "${name}: no checks, no typed functions"
    else
        fail "${name}: ${checked} checks and ${typed} typed functions with cfi off"
    fi
}

# The code pointer half of a function value overwritten from C with another
# function's, of another type: what a memory-safety bug elsewhere would do.
cat > "${WORK}/retype.c" <<'C'
void retype(void **f, void *const *g) { f[0] = g[0]; }
C

make_mismatch_project() {
    local dir="$1" cfi="$2"
    new_project "${dir}" "
[binary.codegen]
cfi = ${cfi}
bti = ${cfi}"

    add_ffi "${dir}" retype "${WORK}/retype.c" <<'SPP'
use std::cffi::CVoid

!ffi
fun retype(f: &mut FunRef[(U64,), U64], g: &FunRef[(Bool,), Bool]) -> CVoid { }
SPP

    cat > "${dir}/src/main.spp" <<'SPP'
use std::llvm::no_inline

!no_inline
fun call(f: FunRef[(U64,), U64]) -> U64 {
  ret f(1_u64)
}

fun add_three(x: U64) -> U64 {
  ret x + 3_u64
}

fun keep(b: Bool) -> Bool {
  ret b
}

fun main() -> Void {
  let mut f: FunRef[(U64,), U64] = add_three
  let g: FunRef[(Bool,), Bool] = keep
  retype::retype(&mut f, &g)
  let s = Str::from(call(f))
  std::console::println(&s)
  std::mem::ops::drop(s)
}
SPP
}

# One case: a call through the retyped value. With the check on it reaches a
# function of another type, so it traps, and the runtime reports a failed check
# rather than the signal, naming nothing of the types involved. With it off,
# nothing is checked, so nothing may be reported as a failed check.
check_mismatch() {
    local mode="$1" cfi="$2"
    local name="${mode}, cfi=${cfi}, a call through a retyped function value"
    local dir="${WORK}/${mode}-${cfi}/retyped"
    make_mismatch_project "${dir}" "${cfi}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    local why=""
    if [[ "${cfi}" == true ]]; then
        if [[ ${RC} -ne 134 ]]; then
            why="exit ${RC}, expected 134"
        elif ! grep -qF "[sppc] check failed: control-flow integrity" "${dir}/run.log"; then
            why="not reported as a failed control-flow-integrity check"
        elif grep -q "fatal signal" "${dir}/run.log"; then
            why="reported as a signal"
        fi
    elif grep -qF "control-flow integrity" "${dir}/run.log"; then
        why="reported as a control-flow-integrity failure with no check emitted"
    fi

    if [[ -z "${why}" ]]; then
        ok "${name} (exit ${RC})"
    else
        fail "${name}: ${why}" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" true
    check "${mode}" false
    check_mismatch "${mode}" true
    check_mismatch "${mode}" false
done

finish
