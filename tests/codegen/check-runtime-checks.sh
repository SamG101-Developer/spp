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
#   - a float that fits once truncated, the lowest S32 included, exits 0;
#   - a division or remainder by zero, and the lowest S32 divided by -1,
#     report their check by name; a division in range exits 0;
#   - the lowest S32 remainder by -1 is 0 (it fits, so it is not an
#     overflow), not a check.
#   - a shift ("<<", ">>", signed ">>", "<<=") by the width or more reports
#     the distance and the width, a distance past the operand's type
#     included (it is not narrowed into range first); one below the width
#     exits 0 with the shifted value.
#
# The division cases need an sppc that names checks 0x16-0x18, and the shift
# cases one that names 0x19; an older one reports "unknown check".
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

fun div_s32(a: S32, b: S32) -> S32 {
  ret a / b
}

fun rem_u64(a: U64, b: U64) -> U64 {
  ret a % b
}

fun rem_s32(a: S32, b: S32) -> S32 {
  ret a % b
}

fun shl_u32(a: U32, by: U32) -> U32 {
  ret a << by
}

fun shr_u8(a: U8, by: U32) -> U8 {
  ret a >> by
}

fun shr_s16(a: S16, by: U32) -> S16 {
  ret a >> by
}

fun shl_assign_u64(a: U64, by: U32) -> U64 {
  let mut x = a
  x <<= by
  ret x
}

fun to_s32(x: F64) -> S32 {
  ret S32::from(x)
}

fun to_u8(x: F64) -> U8 {
  ret U8::from(x)
}

fun nan() -> F64 {
  ret F64::nan()
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
    check "${mode}" "division by zero" "let x = div_s32(1_s32, 0_s32)" 134 \
        "[sppc] check failed: division by zero"
    check "${mode}" "remainder by zero" "let x = rem_u64(1_u64, 0_u64)" 134 \
        "[sppc] check failed: remainder by zero"
    check "${mode}" "lowest S32 divided by -1" "let x = div_s32(-2147483648_s32, -1_s32)" 134 \
        "[sppc] check failed: signed divide overflowed"
    check "${mode}" "division in range" "let x = div_s32(-7_s32, 2_s32)" 0 ""
    check "${mode}" "lowest S32 remainder by -1 is zero" \
        "case rem_s32(-2147483648_s32, -1_s32) == 0_s32 { std::console::println(\"rem zero\") }" 0 "rem zero"
    check "${mode}" "shift left by the width" "let x = shl_u32(1_u32, 32_u32)" 134 \
        "[sppc] check failed: shift distance 32 is not less than the width 32"
    check "${mode}" "shift left past the width" "let x = shl_u32(1_u32, 40_u32)" 134 \
        "[sppc] check failed: shift distance 40 is not less than the width 32"
    check "${mode}" "shift right past the operand's type" "let x = shr_u8(1_u8, 256_u32)" 134 \
        "[sppc] check failed: shift distance 256 is not less than the width 8"
    check "${mode}" "signed shift right by the width" "let x = shr_s16(-2_s16, 16_u32)" 134 \
        "[sppc] check failed: shift distance 16 is not less than the width 16"
    check "${mode}" "shift-assign past the width" "let x = shl_assign_u64(1_u64, 64_u32)" 134 \
        "[sppc] check failed: shift distance 64 is not less than the width 64"
    check "${mode}" "shifts in range" \
        "std::console::println(\"{} {} {} {}\", shl_u32(1_u32, 31_u32), shr_u8(128_u8, 7_u32), shr_s16(-2_s16, 15_u32), shl_assign_u64(1_u64, 63_u32))" \
        0 "2147483648 1 -1 9223372036854775808"
done

finish
