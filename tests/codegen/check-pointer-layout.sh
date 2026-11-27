#!/usr/bin/env bash

# End-to-end check that a raw pointer ("Ptr[T]", so "CVoidPtr") is pointer-sized ("type_predicates::IsTypePointer"):
# it is lowered to a bare llvm pointer, so it measures as one, though it has no fields. When "size_of" measured it
# empty, a class holding one counted as zero-sized, "Vec" never allocated storage for it, and its first "append"
# stored through the empty buffer's dangling pointer. Builds a scratch project whose C library hands back a null
# "CVoidPtr", and asserts, at dev and rel:
#   - "size_of" of a "CVoidPtr", of a class holding one and of a generic class holding one is 8;
#   - a "Vec" of each class takes its appends and keeps its length.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-pointer-layout.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/nullp.c" <<'C'
void *null_handle(void) { return 0; }
C

read -r -d '' PROGRAM <<'EOF'
use std::cffi::CVoidPtr
use std::mem::ops::size_of

cls Holder { !public p: CVoidPtr }
cls Generic[T] { !public p: CVoidPtr }

fun main() -> Void {
  case size_of[CVoidPtr]() == 8_uz { std::console::println("pointer 8") }
  case size_of[Holder]() == 8_uz { std::console::println("holder 8") }
  case size_of[Generic[U8]]() == 8_uz { std::console::println("generic 8") }

  let mut held = Vec[Holder]::new()
  held.append(Holder(p=nullp::null_handle()))
  held.append(Holder(p=nullp::null_handle()))
  held.append(Holder(p=nullp::null_handle()))
  case held.len() == 3_uz { std::console::println("appended 3") }
  std::mem::ops::drop(held)

  let mut generic = Vec[Generic[U8]]::new()
  generic.append(Generic[U8](p=nullp::null_handle()))
  case generic.len() == 1_uz { std::console::println("appended 1") }
  std::mem::ops::drop(generic)
}
EOF

for mode in dev rel; do
    dir="${WORK}/${mode}/pointer"
    name="${mode}, raw pointers are pointer-sized"
    new_project "${dir}"
    add_ffi "${dir}" nullp "${WORK}/nullp.c" <<'SPP'
use std::cffi::CVoidPtr

!ffi
fun null_handle() -> CVoidPtr { }
SPP
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"

    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); continue; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -eq 0 ]] && diff -q <(printf '%s\n' "pointer 8" "holder 8" "generic 8" "appended 3" "appended 1") \
        "${dir}/run.log" > /dev/null; then
        ok "${name}"
    else
        fail "${name}: expected the three sizes and both appends, and exit 0 (exit ${RC})" "${dir}/run.log"
    fi
done

finish
