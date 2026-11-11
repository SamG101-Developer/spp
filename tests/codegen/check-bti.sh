#!/usr/bin/env bash

# End-to-end check that "[binary.codegen] bti" guards an AArch64 program's indirect branches with landing pads
# ("codegen::ApplyBranchTargets"). Builds a scratch project for aarch64-unknown-linux-gnu whose program calls a function
# value and a dense switch, then hands the function value to a C helper that branches 4 bytes into it, past its landing
# pad, and asserts, at dev and rel, run under "qemu-aarch64 -cpu max" (which enforces BTI):
#   - with "bti = true" the executable carries the BTI property (so the loader guards it), its object starts the
#     functions an indirect call can reach with "bti c" (and, at rel, the switch's table targets with "bti j"), the
#     program prints what it computed, and the branch past the pad is reported through the abort path as a failed
#     control-flow-integrity check (exit 134), not as the SIGILL it faulted with (S10.5);
#   - with "bti = false" there is neither the property nor a pad, and the same branch runs on unguarded, so nothing
#     reports a control-flow-integrity failure;
#   - with a toolchain whose "crti.o" has no BTI property (a driver standing in for one, as Ubuntu 24.04's glibc is),
#     "bti" left to its default is left out (the record says why, and the program builds and runs unguarded), and
#     "bti = true" set explicitly fails the build naming the file.
# Skipped without an aarch64 backend (-DSPP_ALL_TARGETS=ON), "aarch64-linux-gnu-gcc", "qemu-aarch64" or the sysroot.
# The report comes from the runtime, so it needs a libsppc built with this check's sppc (SPPC_LIB_AARCH64 replaces
# the vendored one until SPP-STL ships it).
#
#   [SPPC_LIB_AARCH64=path/to/aarch64/libsppc.so] tests/codegen/check-bti.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

readonly TRIPLE=aarch64-unknown-linux-gnu
readonly FOLDER=aarch64-linux-gnu
require_aarch64_cross

# A call through "f"'s code pointer 4 bytes on: past the "bti c" it starts with when it has one.
cat > "${WORK}/landing.c" <<'C'
typedef unsigned long (*fn_t)(void *env, unsigned long x);
unsigned long skip_pad(void *const *f) { return ((fn_t)((char *)f[0] + 4))(f[1], 1); }
C

make_project() {
    local dir="$1" bti="$2"
    # Return addresses unsigned, so a branch past a signing prologue is caught by BTI or not at all.
    new_project "${dir}" "
[binary.codegen]
bti = ${bti}
pac_ret = false"
    if [[ -n "${SPPC_LIB_AARCH64:-}" ]]; then
        cp "${SPPC_LIB_AARCH64}" "${dir}/vcs/std/ffi/sppc/${FOLDER}/libsppc.so"
    fi

    mkdir -p "${dir}/ffi/landing/${FOLDER}"
    aarch64-linux-gnu-gcc -shared -fPIC -O2 -Wl,-z,relro,-z,now,-z,separate-code \
        -o "${dir}/ffi/landing/${FOLDER}/liblanding.so" "${WORK}/landing.c"
    cat > "${dir}/ffi/landing/stub.spp" <<'SPP'
use std::cffi::CULong

!ffi
fun skip_pad(f: &FunRef[(U64,), U64]) -> CULong { }
SPP

    cat > "${dir}/src/main.spp" <<'SPP'
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
    else { 1000_u64 }
  }
}

!no_inline
fun call(f: FunRef[(U64,), U64], x: U64) -> U64 {
  ret f(x)
}

fun add_three(x: U64) -> U64 {
  ret x + 3_u64
}

fun main() -> Void {
  let f: FunRef[(U64,), U64] = add_three
  let mut sum = call(f, black_box(4_u64))
  loop i in std::range::Range[U64]::between(0_u64, 9_u64) {
    sum = sum + pick(black_box(i), black_box(i))
  }
  let s = Str::from(sum)
  std::console::println(&s)
  std::mem::ops::drop(s)
  let t = Str::from(landing::skip_pad(&f))
  std::console::println(&t)
  std::mem::ops::drop(t)
}
SPP
}

check() {
    local mode="$1" bti="$2"
    local name="${mode}, bti=${bti}"
    local dir="${WORK}/${mode}-${bti}/guarded"
    make_project "${dir}" "${bti}"
    if ! (cd "${dir}" && "${SPP}" build -m "${mode}" --target "${TRIPLE}" > "${dir}/build.log" 2>&1); then
        if grep -q 'No llvm backend for target' "${dir}/build.log"; then
            echo "skipped: this spp has no aarch64 backend (build it with -DSPP_ALL_TARGETS=ON)"
            exit 0
        fi
        fail "${name}: build failed" "${dir}/build.log"
        return
    fi
    local exe="${dir}/out/${FOLDER}/${mode}/guarded" obj="${dir}/out/${FOLDER}/${mode}/llvm/spp.o"
    timeout 60 qemu-aarch64 -cpu max -L "${AARCH64_SYSROOT}" "${exe}" > "${dir}/run.log" 2>&1
    local rc=$?

    local property pads_c pads_j
    property="$(readelf -n "${exe}" | grep -c 'AArch64 feature: .*BTI')"
    aarch64-linux-gnu-objdump -d "${obj}" > "${dir}/spp.dis"
    pads_c="$(grep -cE '\sbti\s+c$' "${dir}/spp.dis")"
    pads_j="$(grep -cE '\sbti\s+j$' "${dir}/spp.dis")"

    local why=""
    if ! head -1 "${dir}/run.log" | grep -qx "1167"; then
        why="did not print \"1167\" first"
    elif [[ "${bti}" == true ]]; then
        if (( property == 0 )); then
            why="the executable has no BTI property"
        elif (( pads_c == 0 )); then
            why="no \"bti c\" in the object"
        elif [[ "${mode}" == rel ]] && (( pads_j == 0 )); then
            why="no \"bti j\" on the switch's table targets"
        elif [[ ${rc} -ne 134 ]]; then
            why="exit ${rc}, expected 134"
        elif ! grep -qF "[sppc] check failed: control-flow integrity: an indirect branch reached an instruction that is not a landing pad" "${dir}/run.log"; then
            why="the branch past the landing pad was not reported as a failed control-flow-integrity check"
        fi
    elif (( property > 0 || pads_c > 0 || pads_j > 0 )); then
        why="a BTI property or landing pads with bti off"
    elif grep -qF "control-flow integrity" "${dir}/run.log"; then
        why="reported a control-flow-integrity failure with bti off"
    fi

    if [[ -z "${why}" ]]; then
        ok "${name}: ${pads_c} \"bti c\", ${pads_j} \"bti j\" (exit ${rc})"
    else
        fail "${name}: ${why}" "${dir}/run.log"
    fi
}

# A driver that answers for "crti.o" with an object without the property, and is the real cross gcc otherwise.
aarch64-linux-gnu-gcc -c -fPIC -x c - -o "${WORK}/nobti-crti.o" <<< 'void nobti(void) {}'
cat > "${WORK}/nobti-gcc" <<SH
#!/bin/sh
case "\$1" in -print-file-name=crti.o) echo "${WORK}/nobti-crti.o"; exit 0;; esac
exec aarch64-linux-gnu-gcc "\$@"
SH
chmod +x "${WORK}/nobti-gcc"

check_unmarked_toolchain() {
    local explicit="$1"
    local name="a start file without BTI, bti ${explicit}"
    local dir="${WORK}/unmarked-${explicit// /-}/plain"
    local setting=""
    [[ "${explicit}" == "set true" ]] && setting=$'\n[binary.codegen]\nbti = true'
    new_project "${dir}" "
[target.linkers]
${FOLDER} = \"${WORK}/nobti-gcc\"
${setting}"
    printf 'fun main() -> Void {\n  std::console::println("hello")\n}\n' > "${dir}/src/main.spp"
    (cd "${dir}" && "${SPP}" build -m rel --target "${TRIPLE}" > "${dir}/build.log" 2>&1)
    local rc=$?
    local exe="${dir}/out/${FOLDER}/rel/plain"

    if [[ "${explicit}" == "set true" ]]; then
        if [[ ${rc} -eq 0 ]]; then
            fail "${name}: the build succeeded" "${dir}/build.log"
        elif tr '\r' '\n' < "${dir}/build.log" | grep -qF "the toolchain's crti.o has no BTI property"; then
            ok "${name}: the build failed, naming crti.o"
        else
            fail "${name}: the build failed without naming crti.o" "${dir}/build.log"
        fi
        return
    fi
    if [[ ${rc} -ne 0 ]]; then
        fail "${name}: build failed" "${dir}/build.log"
    elif ! "${SPP}" audit "${exe}" | grep -qxF "binary.codegen.bti = left out (the toolchain's crti.o has no BTI property)"; then
        fail "${name}: the record does not say bti was left out" <("${SPP}" audit "${exe}")
    elif ! timeout 60 qemu-aarch64 -cpu max -L "${AARCH64_SYSROOT}" "${exe}" | grep -qx hello; then
        fail "${name}: did not run"
    else
        ok "${name}: left out, recorded, and the program runs"
    fi
}

for mode in dev rel; do
    check "${mode}" true
    check "${mode}" false
done
check_unmarked_toolchain "default"
check_unmarked_toolchain "set true"

finish
