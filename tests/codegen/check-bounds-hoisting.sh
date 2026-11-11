#!/usr/bin/env bash

# End-to-end check of index checks proven before their loop ("codegen::AddBoundsPasses", llvm_pass_bounds): a loop whose
# index checks compare an induction variable with a length gets one guard before it, a copy without the checks when
# no check can fail, and the original otherwise. Builds scratch projects with four shapes of loop (one array written,
# a step of 2, two arrays of different lengths, a start past 0), and asserts, at dev and rel:
#   - with every index in range, the program prints exactly the right sums (the copy without checks, at rel);
#   - with the loop running one past an array's end, each aborts exactly as an unversioned loop does: exit 134 and
#     "index N out of bounds for length L" for the first index out of range, so the checked copy runs and fails at
#     the same round;
#   - at rel, the program's IR has the guards ("bounds.fits"), so the shapes were versioned and not left as they were.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-bounds-hoisting.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' FUNCTIONS <<'EOF'
use std::hints::black_box

fun show(n: U64) -> Void {
  let s = Str::from(n)
  std::console::println(&s)
  std::mem::ops::drop(s)
}

fun fill(n: USize) -> U64 {
  let mut v = Vec[U64]::new_filled(black_box(8_uz), 0_u64)
  let mut i = 0_uz
  loop i < n {
    v[mut i]@ = U64::from(i) * 2_u64
    i += 1_uz
  }
  let mut sum = 0_u64
  i = 0_uz
  loop i < v.len() {
    sum += v[i]@
    i += 1_uz
  }
  std::mem::ops::drop(v)
  ret sum
}

fun step2(n: USize) -> U64 {
  let v = Vec[U64]::new_filled(black_box(9_uz), 1_u64)
  let mut sum = 0_u64
  let mut i = 0_uz
  loop i < n {
    sum += v[i]@
    i += 2_uz
  }
  std::mem::ops::drop(v)
  ret sum
}

fun pair(n: USize) -> U64 {
  let a = Vec[U64]::new_filled(black_box(8_uz), 3_u64)
  let b = Vec[U64]::new_filled(black_box(6_uz), 4_u64)
  let mut sum = 0_u64
  let mut i = 0_uz
  loop i < n {
    sum += a[i]@ + b[i]@
    i += 1_uz
  }
  std::mem::ops::drop(a)
  std::mem::ops::drop(b)
  ret sum
}

fun offset(n: USize) -> U64 {
  let v = Vec[U64]::new_filled(black_box(8_uz), 5_u64)
  let mut sum = 0_u64
  let mut i = 3_uz
  loop i < n {
    sum += v[i]@
    i += 1_uz
  }
  std::mem::ops::drop(v)
  ret sum
}
EOF

# Build "body" as main at "mode", run it, and compare its exit code and output with what was expected.
check() {
    local mode="$1" name="$2" body="$3" rc_want="$4" expect="$5"
    name="${mode}, ${name}"
    local dir="${WORK}/${mode}-${RANDOM}/bounds"
    new_project "${dir}"
    printf '%s\n\nfun main() -> Void {\n%s\n}\n' "${FUNCTIONS}" "${body}" > "${dir}/src/main.spp"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    local why=""
    if [[ ${RC} -ne ${rc_want} ]]; then
        why="exit ${RC}, expected ${rc_want}"
    elif [[ ${rc_want} -eq 0 ]] && ! diff -q <(printf '%s\n' "${expect}") "${dir}/run.log" > /dev/null; then
        why="printed other than \"${expect}\""
    elif [[ ${rc_want} -ne 0 ]] && ! grep -qF -- "${expect}" "${dir}/run.log"; then
        why="no \"${expect}\" in the output"
    elif grep -q "fatal signal" "${dir}/run.log"; then
        why="ended by a signal, not the abort path"
    elif [[ ${mode} == rel && ${rc_want} -eq 0 ]] \
        && ! grep -rqF "bounds.fits" "$(dirname "${EXE}")/llvm" --include='*.ll'; then
        why="no versioning guard in the IR"
    fi

    if [[ -z "${why}" ]]; then
        ok "${name} (exit ${RC})"
    else
        fail "${name}: ${why}" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" "all in range" "  show(fill(black_box(8_uz)))
  show(step2(black_box(9_uz)))
  show(pair(black_box(6_uz)))
  show(offset(black_box(8_uz)))" 0 "$(printf '56\n5\n42\n25')"
    check "${mode}" "written one past the end" "  show(fill(black_box(9_uz)))" 134 \
        "[sppc] check failed: index 8 out of bounds for length 8"
    check "${mode}" "a step of 2 past the end" "  show(step2(black_box(11_uz)))" 134 \
        "[sppc] check failed: index 10 out of bounds for length 9"
    check "${mode}" "the shorter of two arrays" "  show(pair(black_box(7_uz)))" 134 \
        "[sppc] check failed: index 6 out of bounds for length 6"
    check "${mode}" "a start past 0, one past the end" "  show(offset(black_box(9_uz)))" 134 \
        "[sppc] check failed: index 8 out of bounds for length 8"
done

finish
