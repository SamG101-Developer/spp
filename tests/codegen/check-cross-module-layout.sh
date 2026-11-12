#!/usr/bin/env bash

# End-to-end check that a class is laid out before anything is sized around it, whichever module uses it first
# ("codegen::GetLlvmTypeOf" finishing a class registered as an empty struct; notes: opaque-struct-layout-cache). A
# constant of "Arr[BigInt, 2]" in module "a" registers "BigInt" without laying it out; "b" then lays out
# "Opt[BigInt]", before std's own module lays "BigInt" out. Until 2026-11-12 the payload came out "[0 x i8]", LLVM
# having cached an empty layout for the empty struct, and storing a "BigInt" in it overran the stack ("stack smashing
# detected"). Asserts, at dev and rel:
#   - the program wraps a "BigInt" in an "Opt" in "b" and unwraps it in "main", and exits normally;
#   - in "b"'s IR, the "Opt[BigInt]" payload has room for a "BigInt" (not "[0 x ...]").
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-cross-module-layout.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

make_project() {
    local dir="$1"
    new_project "${dir}"

    # The constant registers "BigInt" (an array's element) without laying it out.
    cat > "${dir}/src/a.spp" <<'SPP'
cmp seeds: std::array::Arr[std::bignum::bigint::BigInt, 2_uz] = std::array::Arr[std::bignum::bigint::BigInt, 2_uz]()
SPP

    # A later module, before std's: "Opt[BigInt]" is laid out around it.
    cat > "${dir}/src/b.spp" <<'SPP'
!public
fun wrap(v: S64) -> Opt[std::bignum::bigint::BigInt] {
  ret Some(val=std::bignum::bigint::BigInt::from(v))
}
SPP

    cat > "${dir}/src/main.spp" <<'SPP'
use std::hints::black_box

fun main() -> Void {
  let held = b::wrap(black_box(40_s64))
  case held of {
    is Some[std::bignum::bigint::BigInt](val) {
      std::console::println("held")
      std::mem::ops::drop(val)
    }
    else { }
  }
}
SPP
}

for mode in dev rel; do
    dir="${WORK}/${mode}/layout"
    make_project "${dir}"
    build_project "${mode}" "${dir}" "${mode}" || { failures=$((failures + 1)); continue; }
    run_exe "${EXE}" "${dir}/run.log"

    payload="$(grep -E '^%"std::variant::Var\[std::option::None, std::option::Some\[T=std::bignum::bigint::BigInt\]\]" = type' \
        "$(dirname "${OBJ}")/b.ll" | sed -E 's/.*= type \{ [^,]*, (.*) \}$/\1/')"
    if (( RC != 0 )) || ! grep -qx "held" "${dir}/run.log"; then
        fail "${mode}: expected \"held\" and exit 0 (exit ${RC})" "${dir}/run.log"
    elif [[ -z "${payload}" || "${payload}" == *"[0 x"* ]]; then
        fail "${mode}: b's Opt[BigInt] payload is \"${payload}\", which cannot hold a BigInt"
    else
        ok "${mode}: Opt[BigInt] laid out in b holds a BigInt (${payload})"
    fi
done

finish
