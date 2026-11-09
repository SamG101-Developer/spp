#!/usr/bin/env bash

# End-to-end check of AIR integers ("[binary.codegen] air_integers",
# .triages/air-integer-model.md): an overflow stops the program before the wrong
# value is observed, not necessarily at the operation; in a loop that does
# nothing observable and can then vectorise, the checks move to its exit.
# Asserts:
#   - a baseline build is left alone (the passes act only on 256-bit vectors);
#   - built for x86-64-v3 (skipped without AVX2): an in-place add over a Vec,
#     in range, prints the right value, its loop deferred ("air.") and
#     vectorised;
#   - the same loop overflowing a signed add aborts with "signed add overflowed";
#   - an unsigned subtract underflowing in it aborts with "unsigned subtract
#     underflowed";
#   - with the key off, the overflow is reported the same, and nothing is
#     deferred;
#   - at dev, where nothing is deferred, the overflow is reported the same;
#   - a running sum is computed exactly: right in range, stopped on overflow;
#   - a fussy overflow (MAX + 1 - 1 in a sum) finishes in rel with the exact
#     result, and stops in dev;
#   - an unknown "[target.floor] cpu" is refused.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-air-integers.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

# A program adding "k" to every element of a 4096-element Vec of "ty", 1000
# times, starting from "start", and printing the first element.
map_body() {
    local ty="$1" start="$2" k="$3" op="$4"
    cat <<EOF
use std::console::println
use std::hints::black_box

fun main() -> Void {
  let mut x = Vec[${ty}]::new_filled(black_box(4096_uz), ${start})
  let k = black_box(${k})
  let mut pass = 0_s64
  loop pass < black_box(1000_s64) {
    let mut i = 0_uz
    loop i < x.len() {
      x[mut i]@ = x[i]@ ${op} k
      i += 1_uz
    }
    pass += 1_s64
  }
  println("{}", x[0_uz]@)
  drop(x)
}
EOF
}

sum_body() {
    local start="$1" extra="${2:-}"
    cat <<EOF
use std::console::println
use std::hints::black_box

fun main() -> Void {
  let mut x = Vec[S64]::new_filled(black_box(4096_uz), black_box(${start}))
  ${extra}
  let mut sum = 0_s64
  let mut i = 0_uz
  loop i < x.len() {
    sum = sum + x[i]@
    i += 1_uz
  }
  println("{}", sum)
  drop(x)
}
EOF
}

# The number of lines in the program's own "main" matching "pattern", in the
# optimised lto.ll of "dir".
count_in_main() {
    local dir="$1" pattern="$2" ir
    ir="$(find "${dir}/out" -type f -path "*/llvm/lto.ll" | head -1)"
    awk -v p="${pattern}" '/^define.*@"main::main/ { on = 1 } on && $0 ~ p { n++ } on && /^}/ { exit } END { print n + 0 }' \
        "${ir}"
}

# One case: build "body" at "mode" with "config", run it, and compare the exit
# code and report (or output) with what was expected.
check() {
    local name="$1" mode="$2" body="$3" rc_want="$4" expect="$5" config="${6:-}"
    local dir="${WORK}/${name//[ ,]/-}/deferred"
    new_project "${dir}" "${config}"
    printf '%s\n' "${body}" > "${dir}/src/main.spp"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return 1; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne ${rc_want} ]]; then
        fail "${name}: exit ${RC}, expected ${rc_want}" "${dir}/run.log"
        return 1
    fi
    if ! grep -qF -- "${expect}" "${dir}/run.log"; then
        fail "${name}: no \"${expect}\" in the output" "${dir}/run.log"
        return 1
    fi
    ok "${name} (exit ${RC})"
    LAST_DIR="${dir}"
}

KEY_OFF="
[binary.codegen]
air_integers = false"

# The AIR passes act only where vectors are at least 256 bits wide, so they are
# checked with the program built for AVX2 ("[target.floor] cpu"), which this
# machine must have to run it. A baseline build is checked to be left alone.
WIDE="
[target.floor]
cpu = \"x86-64-v3\""
if [[ "$(uname -m)" == x86_64 ]] && grep -qw avx2 /proc/cpuinfo 2>/dev/null; then HAS_AVX2=1; else HAS_AVX2=0; fi

if check "in range, baseline" rel "$(map_body S64 1_s64 7_s64 +)" 0 "7001"; then
    deferred="$(count_in_main "${LAST_DIR}" "air[.]")"
    if (( deferred == 0 )); then
        ok "in range, baseline: left alone (vectors under 256 bits)"
    else
        fail "in range, baseline: ${deferred} AIR lines in a baseline build"
    fi
fi

if (( HAS_AVX2 == 0 )); then
    ok "AIR cases skipped: this machine has no AVX2"
    finish
    exit
fi

if check "in range, x86-64-v3" rel "$(map_body S64 1_s64 7_s64 +)" 0 "7001" "${WIDE}"; then
    deferred="$(count_in_main "${LAST_DIR}" "air[.]any")"
    vectors="$(count_in_main "${LAST_DIR}" "<[0-9]+ x ")"
    if (( deferred > 0 && vectors > 0 )); then
        ok "in range, x86-64-v3: the loop is deferred and vectorised (${vectors} vector lines)"
    else
        fail "in range, x86-64-v3: expected the loop deferred and vectorised, got ${deferred} deferred, ${vectors} vector lines"
    fi
fi

check "signed add overflow" rel "$(map_body S64 1_s64 4000000000000000000_s64 +)" 134 \
    "[sppc] check failed: signed add overflowed" "${WIDE}"
check "unsigned subtract underflow" rel "$(map_body U64 5000_u64 7_u64 -)" 134 \
    "[sppc] check failed: unsigned subtract underflowed" "${WIDE}"

if check "signed add overflow, key off" rel "$(map_body S64 1_s64 4000000000000000000_s64 +)" 134 \
    "[sppc] check failed: signed add overflowed" "${WIDE}${KEY_OFF}"; then
    deferred="$(count_in_main "${LAST_DIR}" "air[.]")"
    if (( deferred == 0 )); then
        ok "signed add overflow, key off: nothing deferred"
    else
        fail "signed add overflow, key off: ${deferred} deferred lines with the key off"
    fi
fi

check "signed add overflow, dev" dev "$(map_body S64 1_s64 4000000000000000000_s64 +)" 134 \
    "[sppc] check failed: signed add overflowed" "${WIDE}"

# Running sums: computed exactly, and checked once on the way out.
if check "running sum in range" rel "$(sum_body 3_s64)" 0 "12288" "${WIDE}"; then
    exact="$(count_in_main "${LAST_DIR}" "air[.]sum[.]result")"
    if (( exact > 0 )); then
        ok "running sum in range: computed exactly"
    else
        fail "running sum in range: not computed exactly"
    fi
fi
check "running sum overflow" rel "$(sum_body 4000000000000000000_s64)" 134 \
    "[sppc] check failed: signed add overflowed" "${WIDE}"

# A fussy overflow: the partial sum leaves the range (MAX + 1) and comes back
# (- 1), so the total fits. Dev, in source order, stops; rel, exact, finishes.
FUSSY='x[mut 0_uz]@ = 9223372036854775807_s64
  x[mut 1_uz]@ = 1_s64
  x[mut 2_uz]@ = -1_s64'
check "fussy overflow, rel finishes" rel "$(sum_body 0_s64 "${FUSSY}")" 0 "9223372036854775807" "${WIDE}"
check "fussy overflow, dev stops" dev "$(sum_body 0_s64 "${FUSSY}")" 134 \
    "[sppc] check failed: signed add overflowed" "${WIDE}"

# An unknown CPU is refused, naming where to set it.
dir="${WORK}/unknown-cpu/deferred"
new_project "${dir}" "
[target.floor]
cpu = \"pentium-pro-max\""
printf '%s\n' "$(map_body S64 1_s64 7_s64 +)" > "${dir}/src/main.spp"
if (cd "${dir}" && "${SPP}" build -m rel > "${dir}/build.log" 2>&1); then
    fail "unknown cpu: the build succeeded" "${dir}/build.log"
elif tr '\r' '\n' < "${dir}/build.log" | grep -qF "'pentium-pro-max' is not a CPU of"; then
    ok "unknown cpu: refused"
else
    fail "unknown cpu: refused without naming the CPU" "${dir}/build.log"
fi

finish
