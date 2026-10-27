#!/usr/bin/env bash

# Check that "!ffi" functions are declared as the target's C calling convention takes their parameters, not as their
# LLVM values, each as clang lowers that prototype for the target:
#  - a struct by value: "sppc_pthread_create" takes "sppc_closure" (two pointers), which 32-bit PowerPC takes "byval",
#    s390x as a pointer to a copy, and x86_64 as the struct itself;
#  - a small integer: s390x extends an "int" by its signedness ("sppc_open"'s flags and mode), riscv64 sign-extends
#    either, and x86_64 neither; the "int" returned is extended where its argument is.
# Without either, sppc read garbage on ppc32 and s390x: every thread crashed, and clocks and sockets failed by chance.
# Only emitted, not linked or run, so no cross toolchain is needed; skipped by an spp without those backends (the
# default build: "-DSPP_ALL_TARGETS=ON" links them), so it is run in the cross-codegen lane.
#
#   tests/codegen/check-c-abi.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' PROGRAM <<'SPP'
fun main() -> Void {
    let h = std::threading::thread::spawn(() -> Void { }).expect("spawn failed")
    h.join().expect("join failed")
    let now = std::time::time_point::TimePoint::now()
    let path = std::fs::path::Path::new("/nonexistent")
    let file = std::fs::file::File::open(&path, std::fs::file::FileMode().read())
    drop(file)
    drop(path)
}
SPP

# The hardening keys these targets have no backend support for, as in the cross-codegen lane.
read -r -d '' CONFIG <<'TOML'
[binary.codegen]
sls = false
scrub = false
cfi = false
jump_table_bounds = false

[memory.stack]
split = false
TOML

# Build for "triple", then expect each remaining argument in the declaration of the "sppc_*" function it names.
check() {
    local triple="$1"
    shift
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
    local want fn got
    for want in "$@"; do
        fn="$(grep -oE '@sppc_[a-z_]+' <<< "${want}")"
        got="$(grep -m1 -oE "declare[^@]*${fn}\\([^#]*" "${ir}")"
        if [[ "${got}" == *"${want}"* ]]; then
            ok "${triple}: ${want}..."
        else
            fail "${triple}: want ${want}..., got: ${got:-no declaration}"
        fi
    done
}

check powerpc-unknown-linux-gnu \
    '@sppc_pthread_create(ptr noundef byval({ ptr, ptr }) align 4, '
check s390x-unknown-linux-gnu \
    '@sppc_pthread_create(ptr noundef, ' \
    '@sppc_open(ptr noundef, i32 noundef signext, i32 noundef zeroext, ' \
    'signext i32 @sppc_clock_gettime(i32 noundef signext, '
check riscv64-unknown-linux-gnu \
    '@sppc_open(ptr noundef, i32 noundef signext, i32 noundef signext, ' \
    'signext i32 @sppc_clock_gettime(i32 noundef signext, '
check x86_64-unknown-linux-gnu \
    '@sppc_pthread_create({ ptr, ptr } noundef, ' \
    'noundef i32 @sppc_open(ptr noundef, i32 noundef, i32 noundef, ' \
    'noundef i32 @sppc_clock_gettime(i32 noundef, '

finish
