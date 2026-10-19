#!/usr/bin/env bash

# Check that a build is reproducible: the same project built from two
# differently named directories gives byte-identical output, and none of it
# records where it was built. Asserts, at dev and rel:
#   - the program's object and the linked executable are identical;
#   - neither contains either build directory's path.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-reproducible.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

# Closures, generics, strings, a loop and a "case": enough of the language
# that a nondeterministic name or ordering anywhere would show.
make_project() {
    local dir="$1"
    new_project "${dir}"

    cat > "${dir}/src/main.spp" <<'EOF'
fun pick(n: U64) -> U64 {
  ret case n of {
    == 0_u64 { 11_u64 }
    == 1_u64 { 23_u64 }
    else { 0_u64 }
  }
}

fun main() -> Void {
  let f = (x: U64) pick(x)
  let mut sum = f(1_u64)
  loop i in std::range::Range[U64]::between(0_u64, 3_u64) {
    sum = sum + pick(i)
  }
  let v = Vec[U64]::new_filled(3_uz, sum)
  let s = Str::from(v.len())
  std::console::println(&s)
  std::mem::ops::drop(s)
  std::mem::ops::drop(v)
}
EOF
}

check() {
    local mode="$1"
    # Different lengths as well as names, so an embedded path would also
    # shift every offset after it.
    local a="${WORK}/${mode}/a/repro" b="${WORK}/${mode}/somewhere-else/repro"
    make_project "${a}"
    make_project "${b}"
    build_project "${mode}, ${a}" "${a}" "${mode}" || { failures=$((failures + 1)); return; }
    local exe_a="${EXE}" obj_a="${OBJ}"
    build_project "${mode}, ${b}" "${b}" "${mode}" || { failures=$((failures + 1)); return; }
    local exe_b="${EXE}" obj_b="${OBJ}"

    local why=""
    local pair
    for pair in "spp.o ${obj_a} ${obj_b}" "repro ${exe_a} ${exe_b}"; do
        local what fa fb
        read -r what fa fb <<< "${pair}"
        if ! cmp -s "${fa}" "${fb}"; then
            why="${why} ${what} differs ($(cmp -l "${fa}" "${fb}" | wc -l) bytes);"
        elif grep -qaF -e "${WORK}" "${fa}"; then
            why="${why} ${what} records the build path;"
        fi
    done

    if [[ -z "${why}" ]]; then
        ok "${mode}: object and executable identical, no build path"
    else
        fail "${mode}:${why}"
    fi
}

for mode in dev rel; do
    check "${mode}"
done

finish
