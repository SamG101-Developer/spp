#!/usr/bin/env bash

# End-to-end check of the float checks ("[binary.codegen] float_checks"): a
# float operation that makes up a value ends the process through the one abort
# path, naming the kind, the width and the operation: a checked operation stops
# on any NaN or infinite result, whatever its operands (a value made non-finite
# on purpose goes through the "*_ieee" forms). Asserts, at dev and rel, with the
# checks on:
#   - "0/0", "inf - inf", "x % 0" and "sqrt(-1)" report an invalid operation;
#   - a finite value divided by zero reports a division by zero;
#   - "MAX * 2", an F64 narrowed past F32's range and the largest U128 into an
#     F32 report an overflow;
#   - arithmetic in range exits 0 and reports nothing;
#   - a NaN or infinity going into a checked operation stops it ("inf + 1",
#     "NaN * 0", "sqrt(inf)", an infinity narrowed);
#   - the maths functions stop as the arithmetic does, naming the function: a
#     NaN result is invalid ("ln(-1)", "asin(2)", "acosh(0.5)"), an infinity at
#     a pole a division by zero ("ln(0)", "pow(0, -1)", "atanh(1)", whose own
#     division names it), any other an overflow ("exp(1000)"); in range, and
#     through the IEEE forms at a pole, they report nothing;
#   - an element-wise loop ("a[i] = a[i] * b[i] + 1"), in range, prints the
#     right value, and at rel its checks are deferred ("air.") and it is
#     vectorised, at any vector width (float compares vectorise on SSE2);
#   - the same loop with an element overflowing, or a NaN going in, reports
#     the overflow or the invalid operation at dev; at rel, where its checks
#     are deferred and the multiply's is left to the add's, "not finite", at
#     the add;
#   - "a * b + 1" is checked once at rel: an overflowing multiply is reported
#     at the add (at dev, at the multiply); a division keeps its own check, so
#     "1 / 0 + 1" is a division by zero at both; "sqrt" passes a NaN or an
#     infinity on too, so "sqrt(a * b + 1)" is checked at the "sqrt"; a
#     multiply compared, or used
#     as a divisor (either can lose an infinity), keeps its own check;
#   - the loop over "Vec"s indexed up to one's length (an index check the
#     bounds pass versions the loop by) is deferred and vectorised too;
# and with the checks off ("float_checks = false"; on is the default), "1/0" is
# IEEE's infinity, no report.
#
# The reports need an sppc that names checks 0x20-0x23 and the maths functions
# (operations 8-24); an older one reports "unknown check", or "(F64 operation)".
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-float-checks.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

readonly ON='[binary.codegen]
float_checks = true'
readonly OFF='[binary.codegen]
float_checks = false'

make_project() {
    local dir="$1" body="$2" config="$3"
    new_project "${dir}" "${config}"

    cat > "${dir}/src/main.spp" <<EOF
fun add(a: F64, b: F64) -> F64 {
  ret a + b
}

fun sub(a: F64, b: F64) -> F64 {
  ret a - b
}

fun mul(a: F64, b: F64) -> F64 {
  ret a * b
}

fun div(a: F64, b: F64) -> F64 {
  ret a / b
}

fun div32(a: F32, b: F32) -> F32 {
  ret a / b
}

fun rem(a: F64, b: F64) -> F64 {
  ret a % b
}

fun root(a: F64) -> F64 {
  ret a.sqrt()
}

fun narrow(a: F64) -> F32 {
  ret F32::from(a)
}

fun from_u128(a: U128) -> F32 {
  ret F32::from(a)
}

fun bits(x: F64) -> U64 {
  ret std::intrinsics::bit_cast[U64](x)
}

fun inf() -> F64 {
  ret std::intrinsics::bit_cast[F64](9218868437227405312_u64)
}

fun nan() -> F64 {
  ret std::intrinsics::bit_cast[F64](9221120237041090560_u64)
}

fun main() -> Void {
  ${body}
}
EOF
}

# One case: build, run, and compare the exit code and report with what was
# expected. An empty "expect" means no report at all.
check() {
    local mode="$1" name="$2" body="$3" rc_want="$4" expect="$5" config="${6-${ON}}"
    name="${mode}, ${name}"
    local dir="${WORK}/${mode}-${RANDOM}/checks"
    make_project "${dir}" "${body}" "${config}"
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

readonly INVALID="[sppc] check failed: float invalid operation: the result is a NaN"
readonly OVERFLOW="[sppc] check failed: float overflow: the result is infinite"
readonly NOT_FINITE="[sppc] check failed: float invalid operation or overflow: the result is a NaN or infinite"

# The element-wise loop: "a" filled with "fill", "b" with 2.0 but for element
# 700, "odd"; prints element 700 of "a" after "a[i] = a[i] * b[i] + 1". In
# "Arr"s, or (for "vec") "Vec"s indexed up to "a"'s length: "b[i]" keeps an
# index check, which the bounds pass versions the loop by.
make_loop_project() {
    local dir="$1" fill="$2" odd="$3" container="$4"
    new_project "${dir}" "${ON}"
    if [[ "${container}" == vec ]]; then
        cat > "${dir}/src/main.spp" <<EOF
use std::hints::black_box

!std::llvm::no_inline
fun scale(a: &mut Vec[F64], b: &Vec[F64]) -> Void {
  let mut i = 0_uz
  loop i < a.len() {
    a[mut i]@ = a[i]@ * b[i]@ + 1.0_f64
    i += 1_uz
  }
}

fun main() -> Void {
  let mut a = Vec[F64]::new_filled(black_box(1024_uz), black_box(${fill}))
  let mut b = Vec[F64]::new_filled(black_box(1024_uz), black_box(2.0_f64))
  b[mut 700_uz]@ = black_box(${odd})
  scale(&mut a, &b)
  std::console::println("{}", std::intrinsics::bit_cast[U64](a[700_uz]@))
  drop(a)
  drop(b)
}
EOF
        return
    fi
    cat > "${dir}/src/main.spp" <<EOF
use std::hints::black_box

!std::llvm::no_inline
fun scale(a: &mut Arr[F64, 1024_uz], b: &Arr[F64, 1024_uz]) -> Void {
  let mut i = 0_uz
  loop i < 1024_uz {
    a[mut i]@ = a[i]@ * b[i]@ + 1.0_f64
    i += 1_uz
  }
}

fun main() -> Void {
  let mut a = [black_box(${fill}); 1024_uz]
  let mut b = [black_box(2.0_f64); 1024_uz]
  b[mut 700_uz]@ = black_box(${odd})
  scale(&mut a, &b)
  std::console::println("{}", std::intrinsics::bit_cast[U64](a[700_uz]@))
}
EOF
}

# One loop case, as "check" does; "vectorised" also asks that the rel build's
# loop is deferred and vectorised.
check_loop() {
    local mode="$1" name="$2" fill="$3" odd="$4" rc_want="$5" expect="$6" vectorised="${7:-}" container="${8:-arr}"
    name="${mode}, loop over ${container}s: ${name}"
    local dir="${WORK}/${mode}-${RANDOM}/loop"
    make_loop_project "${dir}" "${fill}" "${odd}" "${container}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne ${rc_want} ]] || ! grep -qF "${expect}" "${dir}/run.log"; then
        fail "${name}: exit ${RC} (expected ${rc_want}), \"${expect}\" wanted" "${dir}/run.log"
        return
    fi
    if [[ -n "${vectorised}" ]]; then
        local ir scale deferred vectors
        ir="$(find "${dir}/out" -name lto.ll -path "*/rel/*" | head -1)"
        scale="$(awk '/^define .*main::scale/,/^}/' "${ir}")"
        deferred="$(grep -c 'air\.' <<< "${scale}")"
        vectors="$(grep -cE '<[0-9]+ x double>' <<< "${scale}")"
        if (( deferred == 0 || vectors == 0 )); then
            fail "${name}: expected the loop deferred and vectorised, got ${deferred} deferred, ${vectors} vector lines"
            return
        fi
        ok "${name} (exit ${RC}; deferred and vectorised, ${vectors} vector lines)"
        return
    fi
    ok "${name} (exit ${RC})"
}

readonly MAX_F64="179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0_f64"

for mode in dev rel; do
    check "${mode}" "zero divided by zero" "let x = div(0.0_f64, 0.0_f64)" 134 "${INVALID} (F64 divide)"
    check "${mode}" "infinity minus infinity" "let x = sub(inf(), inf())" 134 "${INVALID} (F64 subtract)"
    check "${mode}" "remainder by zero" "let x = rem(5.0_f64, 0.0_f64)" 134 "${INVALID} (F64 remainder)"
    check "${mode}" "square root of -1" "let x = root(-1.0_f64)" 134 "${INVALID} (F64 square root)"
    check "${mode}" "division by zero" "let x = div(1.0_f64, 0.0_f64)" 134 \
        "[sppc] check failed: float division by zero (F64 divide)"
    check "${mode}" "F32 division by zero" "let x = div32(-1.0_f32, 0.0_f32)" 134 \
        "[sppc] check failed: float division by zero (F32 divide)"
    check "${mode}" "a multiply then an add, checked once at rel" \
        "let x = add(mul(std::hints::black_box(${MAX_F64}), 2.0_f64), 1.0_f64)" 134 \
        "${OVERFLOW} (F64 $([[ "${mode}" == rel ]] && echo add || echo multiply))"
    check "${mode}" "a multiply then a square root, checked once at rel" \
        "let x = root(add(mul(std::hints::black_box(${MAX_F64}), 2.0_f64), 1.0_f64))" 134 \
        "${OVERFLOW} (F64 $([[ "${mode}" == rel ]] && echo 'square root' || echo multiply))"
    check "${mode}" "a multiply then a comparison keeps its check" \
        "let x = mul(std::hints::black_box(${MAX_F64}), 2.0_f64) > 0.0_f64" 134 "${OVERFLOW} (F64 multiply)"
    check "${mode}" "a multiply used as a divisor keeps its check" \
        "let x = div(1.0_f64, mul(std::hints::black_box(${MAX_F64}), 2.0_f64))" 134 "${OVERFLOW} (F64 multiply)"
    check "${mode}" "a division by zero then an add, still a division by zero" \
        "let x = add(div(1.0_f64, std::hints::black_box(0.0_f64)), 1.0_f64)" 134 \
        "[sppc] check failed: float division by zero (F64 divide)"
    check "${mode}" "multiply overflowed" \
        "let x = mul(179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0_f64, 2.0_f64)" \
        134 "${OVERFLOW} (F64 multiply)"
    check "${mode}" "F64 narrowed past F32" "let x = narrow(1000000000000000000000000000000000000000000.0_f64)" 134 \
        "${OVERFLOW} (F32 conversion)"
    check "${mode}" "largest U128 into F32" "let x = from_u128(340282366920938463463374607431768211455_u128)" 134 \
        "${OVERFLOW} (F32 conversion)"
    check "${mode}" "arithmetic in range" \
        "std::console::println(\"{} {}\", bits(div(1.0_f64, 4.0_f64)), bits(root(16.0_f64)))" 0 \
        "4598175219545276416 4616189618054758400"
    check "${mode}" "an infinity going into an add" "let x = add(inf(), 1.0_f64)" 134 "${OVERFLOW} (F64 add)"
    check "${mode}" "a NaN going into a multiply" "let x = mul(nan(), 0.0_f64)" 134 "${INVALID} (F64 multiply)"
    check "${mode}" "square root of infinity" "let x = root(inf())" 134 "${OVERFLOW} (F64 square root)"
    check "${mode}" "an infinity narrowed" "let x = narrow(inf())" 134 "${OVERFLOW} (F32 conversion)"
    check "${mode}" "exp past the range" "let x = std::hints::black_box(1000.0_f64).exp()" 134 "${OVERFLOW} (F64 exp)"
    check "${mode}" "ln of zero" "let x = std::hints::black_box(0.0_f64).ln()" 134 \
        "[sppc] check failed: float division by zero (F64 log)"
    check "${mode}" "ln of a negative" "let x = std::hints::black_box(-1.0_f64).ln()" 134 "${INVALID} (F64 log)"
    check "${mode}" "asin outside its domain" "let x = std::hints::black_box(2.0_f64).asin()" 134 "${INVALID} (F64 asin)"
    check "${mode}" "zero to a negative power" \
        "let x = std::hints::black_box(0.0_f64).pow(std::hints::black_box(-1.0_f64))" 134 \
        "[sppc] check failed: float division by zero (F64 pow)"
    check "${mode}" "atanh of 1" "let x = std::hints::black_box(1.0_f64).atanh()" 134 \
        "[sppc] check failed: float division by zero (F64 divide)"
    check "${mode}" "acosh below 1" "let x = std::hints::black_box(0.5_f64).acosh()" 134 "${INVALID} (F64 square root)"
    check "${mode}" "maths in range, and the IEEE forms at a pole" \
        "std::console::println(\"{} {} {}\", bits(std::hints::black_box(1.0_f64).ln()), bits(std::hints::black_box(0.0_f64).ln_ieee()), bits(std::hints::black_box(2.0_f64).asinh_ieee().sub_ieee(std::hints::black_box(2.0_f64).asinh())))" \
        0 "0 18442240474082181120 0"
    check "${mode}" "checks off: division by zero is infinity" \
        "std::console::println(\"{}\", bits(div(1.0_f64, 0.0_f64)))" 0 "9218868437227405312" "${OFF}"
    check_loop "${mode}" "in range" "1.5_f64" "3.0_f64" 0 "4617878467915022336" \
        "$([[ "${mode}" == rel ]] && echo vectorised)"
    check_loop "${mode}" "in range" "1.5_f64" "3.0_f64" 0 "4617878467915022336" \
        "$([[ "${mode}" == rel ]] && echo vectorised)" vec
    if [[ "${mode}" == rel ]]; then
        check_loop "${mode}" "an element overflows" "${MAX_F64}" "4.0_f64" 134 "${NOT_FINITE} (F64 add)" "" vec
        check_loop "${mode}" "an element overflows" "${MAX_F64}" "4.0_f64" 134 "${NOT_FINITE} (F64 add)"
        check_loop "${mode}" "a NaN going in" \
            "std::intrinsics::bit_cast[F64](9221120237041090560_u64)" "2.0_f64" 134 "${NOT_FINITE} (F64 add)"
    else
        check_loop "${mode}" "an element overflows" "${MAX_F64}" "4.0_f64" 134 "${OVERFLOW} (F64 multiply)" "" vec
        check_loop "${mode}" "an element overflows" "${MAX_F64}" "4.0_f64" 134 "${OVERFLOW} (F64 multiply)"
        check_loop "${mode}" "a NaN going in" \
            "std::intrinsics::bit_cast[F64](9221120237041090560_u64)" "2.0_f64" 134 "${INVALID} (F64 multiply)"
    fi
done

finish
