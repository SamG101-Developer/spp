#!/usr/bin/env bash

# End-to-end check that a class's convention decides its layout ("codegen::Abi::LayOutStruct"). A class is laid out
# the s++ way by default, its attributes re-ordered by alignment, largest first, to remove padding; an "!ffi" class,
# or one marked "!conv("C")", keeps them in the order written, as a c struct does. Builds a scratch project with the
# same three attributes (U8, U64, U16) in four classes, and asserts, at dev and rel:
#   - the program writes and reads every attribute of each back correctly, so each class's accesses agree with its
#     own layout;
#   - in the IR, the "!ffi" and "!conv("C")" classes are "{ i8, i64, i16 }" and the default and "!conv("spp")" ones
#     "{ i64, i16, i8 }".
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-ffi-layout.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' PROGRAM <<'EOF'
use std::hints::black_box

!ffi
cls Ffi {
  !public a: U8
  !public b: U64
  !public c: U16
}

!conv("C")
cls InOrder {
  !public a: U8
  !public b: U64
  !public c: U16
}

cls Default {
  !public a: U8
  !public b: U64
  !public c: U16
}

!conv("spp")
cls Named {
  !public a: U8
  !public b: U64
  !public c: U16
}

fun show(n: U64) -> Void {
  let s = Str::from(n)
  std::console::println(&s)
  std::mem::ops::drop(s)
}

fun main() -> Void {
  let Ffi(a, b, c) = black_box(Ffi(a=1_u8, b=20000000000_u64, c=300_u16))
  show(U64::from(a) + b + U64::from(c))
  let InOrder(a, b, c) = black_box(InOrder(a=2_u8, b=20000000000_u64, c=300_u16))
  show(U64::from(a) + b + U64::from(c))
  let Default(a, b, c) = black_box(Default(a=3_u8, b=20000000000_u64, c=300_u16))
  show(U64::from(a) + b + U64::from(c))
  let Named(a, b, c) = black_box(Named(a=4_u8, b=20000000000_u64, c=300_u16))
  show(U64::from(a) + b + U64::from(c))
}
EOF

# The body of the struct type named "cls" in the program's IR, as "{ ... }".
struct_body() {
    local llvm="$1" cls="$2"
    grep -rhE "^%\"?[^ ]*${cls}[^ ]* = type \{" "${llvm}" --include='*.ll' | head -1 | sed -E 's/^[^=]*= type //'
}

for mode in dev rel; do
    dir="${WORK}/${mode}/layout"
    name="${mode}, class layout by convention"
    new_project "${dir}"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); continue; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -ne 0 ]] || ! diff -q <(printf '20000000301\n20000000302\n20000000303\n20000000304\n') "${dir}/run.log" > /dev/null; then
        fail "${name}: expected the four sums and exit 0 (exit ${RC})" "${dir}/run.log"
        continue
    fi

    llvm="$(dirname "${EXE}")/llvm"
    why=""
    for cls in Ffi InOrder; do
        body="$(struct_body "${llvm}" "${cls}")"
        [[ "${body}" == "{ i8, i64, i16 }" ]] || why="${why} ${cls} is \"${body}\", not in declaration order;"
    done
    for cls in Default Named; do
        body="$(struct_body "${llvm}" "${cls}")"
        [[ "${body}" == "{ i64, i16, i8 }" ]] || why="${why} ${cls} is \"${body}\", not re-ordered;"
    done
    if [[ -z "${why}" ]]; then
        ok "${name}"
    else
        fail "${name}:${why}"
    fi
done

finish
