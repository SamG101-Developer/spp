#!/usr/bin/env bash

# End-to-end check that a failed runtime check ends the process through the one
# abort path ("sppc_check_failed"): the check named with its values, then
# "_exit(134)" rather than a signal. Asserts, at dev and rel:
#   - an out-of-bounds index reports "index 5 out of bounds for length 3";
#   - an in-bounds index exits 0 and reports nothing;
#   - "std::process::abort" takes the same path, not "SIGABRT" and its handler.
#   - an arithmetic overflow reports its check by name, not "SIGILL";
#   - arithmetic in range exits 0 and reports nothing;
#   - a slice ending before it starts, or past the view, reports both bounds;
#   - a slice in bounds exits 0 and reports nothing;
#   - a float converted to an integer it does not fit (too large, negative
#     for an unsigned one, NaN) reports the integer type;
#   - a float that fits once truncated, the lowest S32 included, exits 0.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-runtime-checks.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

make_project() {
    local dir="$1" body="$2"
    new_project "${dir}"

    cat > "${dir}/src/main.spp" <<EOF
fun at(i: USize) -> U64 {
  let a = [1_u64, 2_u64, 3_u64]
  ret a[i]@
}

fun sl(f: USize, u: USize) -> USize {
  let a = [1_u64, 2_u64, 3_u64]
  let s = a.slice_ref(f, u)
  ret s.len()
}

fun add(a: S32, b: S32) -> S32 {
  ret a + b
}

fun sub(a: U64, b: U64) -> U64 {
  ret a - b
}

fun to_s32(x: F64) -> S32 {
  ret S32::from(x)
}

fun to_u8(x: F64) -> U8 {
  ret U8::from(x)
}

fun nan() -> F64 {
  let z = 0.0_f64
  ret z / z
}

fun main() -> Void {
  ${body}
}
EOF
}

# One case: build, run, and compare the exit code and report with what was
# expected. An empty "expect" means no report at all.
check() {
    local mode="$1" name="$2" body="$3" rc_want="$4" expect="$5"
    name="${mode}, ${name}"
    local dir="${WORK}/${mode}-${RANDOM}/checks"
    make_project "${dir}" "${body}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    local why=""
    if [[ ${RC} -ne ${rc_want} ]]; then
        why="exit ${RC}, expected ${rc_want}"
    elif [[ -z "${expect}" ]] && grep -q "\[sppc\]" "${dir}/run.log"; then
        why="reported when nothing should have"
    elif [[ -n "${expect}" ]] && ! grep -qF "${expect}" "${dir}/run.log"; then
        why="no \"${expect}\" in the report"
    elif grep -q "fatal signal" "${dir}/run.log"; then
        why="ended by a signal, not the abort path"
    fi

    if [[ -z "${why}" ]]; then
        ok "${name} (exit ${RC})"
    else
        fail "${name}: ${why}" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" "index out of bounds" "let x = at(5_uz)" 134 \
        "[sppc] check failed: index 5 out of bounds for length 3"
    check "${mode}" "index in bounds" "let x = at(2_uz)" 0 ""
    check "${mode}" "std::process::abort" "std::process::abort()" 134 "[sppc] aborted"
    check "${mode}" "signed add overflow" "let x = add(2147483647_s32, 1_s32)" 134 \
        "[sppc] check failed: signed add overflowed"
    check "${mode}" "unsigned subtract underflow" "let x = sub(0_u64, 1_u64)" 134 \
        "[sppc] check failed: unsigned subtract underflowed"
    check "${mode}" "arithmetic in range" "let x = add(1_s32, 2_s32)" 0 ""
    check "${mode}" "slice ends before it starts" "let x = sl(2_uz, 1_uz)" 134 \
        "[sppc] check failed: slice 2..1 ends before it starts"
    check "${mode}" "slice end out of bounds" "let x = sl(0_uz, 100_uz)" 134 \
        "[sppc] check failed: slice end 100 out of bounds for length 3"
    check "${mode}" "slice in bounds" "let x = sl(1_uz, 3_uz)" 0 ""
    check "${mode}" "float too large for S32" "let x = to_s32(100000000000.0_f64)" 134 \
        "[sppc] check failed: float out of range for S32, or NaN"
    check "${mode}" "negative float for U8" "let x = to_u8(-1.5_f64)" 134 \
        "[sppc] check failed: float out of range for U8, or NaN"
    check "${mode}" "NaN for S32" "let x = to_s32(nan())" 134 \
        "[sppc] check failed: float out of range for S32, or NaN"
    check "${mode}" "float truncating to the lowest S32" "let x = to_s32(-2147483648.9_f64)" 0 ""
    check "${mode}" "float in range" "let x = to_s32(3.7_f64)" 0 ""
done

finish
