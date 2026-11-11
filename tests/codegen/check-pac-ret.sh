#!/usr/bin/env bash

# End-to-end check that "[binary.codegen] pac_ret" signs an AArch64 program's return addresses ("codegen::
# ApplyReturnSigning"). Builds a scratch project for aarch64-unknown-linux-gnu in which a function lends a stack byte
# to a C helper that writes 64 bytes from it, over the return addresses saved above it, with the canary and the split
# stack (which would catch or prevent that first) off, and asserts, at dev and rel, run under "qemu-aarch64 -cpu
# max" (which has FEAT_FPAC, so a failed authentication faults on the instruction itself):
#   - with "pac_ret = true" the functions that save a return address sign it ("paciasp") and authenticate it, the
#     program runs until a corrupted one is authenticated, and that is reported through the abort path as a failed
#     control-flow-integrity check (exit 134), not as the SIGILL it faulted with (S10.5);
#   - with "pac_ret = false" nothing is signed, the program returns to the overwritten address, and nothing reports a
#     failed authentication.
# Skipped without an aarch64 backend (-DSPP_ALL_TARGETS=ON), "aarch64-linux-gnu-gcc", "qemu-aarch64" or the sysroot.
# The report comes from the runtime, so it needs a libsppc built with this check's sppc (SPPC_LIB_AARCH64 replaces
# the vendored one until SPP-STL ships it).
#
#   [SPPC_LIB_AARCH64=path/to/aarch64/libsppc.so] tests/codegen/check-pac-ret.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

readonly TRIPLE=aarch64-unknown-linux-gnu
readonly FOLDER=aarch64-linux-gnu
require_aarch64_cross

cat > "${WORK}/smash.c" <<'C'
#include <stdint.h>
void smash(uint8_t *p, uint64_t n) { for (uint64_t i = 0; i < n; ++i) { p[i] = 0x41; } }
C

make_project() {
    local dir="$1" pac_ret="$2"
    # Nothing but the signature may stand between the overflow and a return address: no canary, no split stack. The
    # helper writes through a pointer of unknown size, which nothing can fortify.
    new_project "${dir}" "
[binary.codegen]
pac_ret = ${pac_ret}

[memory.stack]
protect = false
split = false

[binary.link]
fortified_ffi = false"
    if [[ -n "${SPPC_LIB_AARCH64:-}" ]]; then
        cp "${SPPC_LIB_AARCH64}" "${dir}/vcs/std/ffi/sppc/${FOLDER}/libsppc.so"
    fi

    mkdir -p "${dir}/ffi/smash/${FOLDER}"
    aarch64-linux-gnu-gcc -shared -fPIC -O1 -Wl,-z,relro,-z,now,-z,separate-code \
        -o "${dir}/ffi/smash/${FOLDER}/libsmash.so" "${WORK}/smash.c"
    cat > "${dir}/ffi/smash/stub.spp" <<'SPP'
use std::cffi::CULong
use std::cffi::CVoid

!ffi
fun smash(p: &mut U8, n: CULong) -> CVoid { }
SPP

    cat > "${dir}/src/main.spp" <<'SPP'
use std::hints::black_box
use std::llvm::no_inline

!no_inline
fun victim(n: U64) -> Void {
  let mut x = 0_u8
  smash::smash(&mut x, n)
}

fun main() -> Void {
  victim(black_box(0_u64))
  std::console::println("before")
  victim(black_box(64_u64))
}
SPP
}

check() {
    local mode="$1" pac_ret="$2"
    local name="${mode}, pac_ret=${pac_ret}"
    local dir="${WORK}/${mode}-${pac_ret}/signed"
    make_project "${dir}" "${pac_ret}"
    if ! (cd "${dir}" && "${SPP}" build -m "${mode}" --target "${TRIPLE}" > "${dir}/build.log" 2>&1); then
        if grep -q 'No llvm backend for target' "${dir}/build.log"; then
            echo "skipped: this spp has no aarch64 backend (build it with -DSPP_ALL_TARGETS=ON)"
            exit 0
        fi
        fail "${name}: build failed" "${dir}/build.log"
        return
    fi
    local exe="${dir}/out/${FOLDER}/${mode}/signed" obj="${dir}/out/${FOLDER}/${mode}/llvm/spp.o"
    timeout 60 qemu-aarch64 -cpu max -L "${AARCH64_SYSROOT}" "${exe}" > "${dir}/run.log" 2>&1
    local rc=$?
    local signed
    signed="$(aarch64-linux-gnu-objdump -d "${obj}" | grep -cE '\spaciasp$')"

    local why=""
    if ! head -1 "${dir}/run.log" | grep -qx "before"; then
        why="did not print \"before\" first"
    elif [[ "${pac_ret}" == true ]]; then
        if (( signed == 0 )); then
            why="no \"paciasp\" in the object"
        elif [[ ${rc} -ne 134 ]]; then
            why="exit ${rc}, expected 134"
        elif ! grep -qF "[sppc] check failed: control-flow integrity: a return address failed authentication" "${dir}/run.log"; then
            why="the overwritten return address was not reported as failing authentication"
        elif grep -q "fatal signal" "${dir}/run.log"; then
            why="reported as a signal"
        fi
    elif (( signed > 0 )); then
        why="${signed} \"paciasp\" with pac_ret off"
    elif [[ ${rc} -eq 0 ]]; then
        why="returned to the overwritten address and exited 0"
    elif grep -qF "failed authentication" "${dir}/run.log"; then
        why="reported a failed authentication with pac_ret off"
    fi

    if [[ -z "${why}" ]]; then
        ok "${name}: ${signed} \"paciasp\" (exit ${rc})"
    else
        fail "${name}: ${why}" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" true
    check "${mode}" false
done

finish
