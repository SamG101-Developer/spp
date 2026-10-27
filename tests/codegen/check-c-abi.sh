#!/usr/bin/env bash

# Check that an "!ffi" function's struct parameter is declared as the target's C calling convention takes it, not as
# its LLVM value. "sppc_pthread_create" takes "sppc_closure" (two pointers) by value: 32-bit PowerPC takes it "byval",
# s390x as a pointer to a copy, and x86_64 (the control) as the struct itself, each as clang lowers that prototype.
# With the struct passed as its fields, sppc read it as a pointer on ppc32 and s390x, and every thread crashed. Only
# emitted, not linked or run, so no cross toolchain is needed; skipped by an spp without those backends (the default
# build: "-DSPP_ALL_TARGETS=ON" links them), so it is run in the cross-codegen lane.
#
#   tests/codegen/check-c-abi.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' PROGRAM <<'SPP'
fun main() -> Void {
    let h = std::threading::thread::spawn(() -> Void { }).expect("spawn failed")
    h.join().expect("join failed")
}
SPP

# The hardening keys 32-bit PowerPC and s390x have no backend support for, as in the cross-codegen lane.
read -r -d '' CONFIG <<'TOML'
[binary.codegen]
sls = false
scrub = false
cfi = false
jump_table_bounds = false

[memory.stack]
split = false
TOML

check() {
    local triple="$1" want="$2"
    local dir="${WORK}/c-abi-${triple%%-*}/cabi"
    new_project "${dir}" "${CONFIG}"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    # The link may fail without the target's gcc; only the IR is read.
    (cd "${dir}" && "${SPP}" build -m rel --target "${triple}" > "${dir}/build.log" 2>&1)
    if grep -q 'No llvm backend for target' "${dir}/build.log"; then
        echo "skipped: this spp has no ${triple%%-*} backend (build it with -DSPP_ALL_TARGETS=ON)"
        exit 0
    fi
    local ir
    ir="$(find "${dir}/out" -type f -path '*/rel/llvm/lto.ll' | head -1)"
    if [[ -z "${ir}" ]]; then
        fail "${triple}: no IR emitted" "${dir}/build.log"
        return
    fi
    local got
    got="$(grep -m1 -oE 'declare[^@]*@sppc_pthread_create\([^#]*' "${ir}")"
    if [[ "${got}" == *"@sppc_pthread_create(${want}"* ]]; then
        ok "${triple}: sppc_pthread_create(${want}...)"
    else
        fail "${triple}: want sppc_pthread_create(${want}...), got: ${got:-no declaration}"
    fi
}

check powerpc-unknown-linux-gnu 'ptr noundef byval({ ptr, ptr }) align 4, '
check s390x-unknown-linux-gnu 'ptr noundef, '
check x86_64-unknown-linux-gnu '{ ptr, ptr } noundef, '

finish
