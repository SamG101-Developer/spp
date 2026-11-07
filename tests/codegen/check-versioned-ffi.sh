#!/usr/bin/env bash

# End-to-end check that an ffi library may carry its ELF version
# ("libhi.so.2"), and that one ffi library may need another. Two packages: "lo",
# whose library is "liblo.so.1.2.3", and "hi", whose "libhi.so.2" needs
# "liblo.so.1.2.3" and calls it. The program calls "hi" alone. Asserts:
#   - it builds and runs: both libraries are staged beside the executable under
#     their versioned names, and the executable needs both, so "lo" is loaded
#     from there even though the program never calls it (no "--as-needed"
#     dropping it, leaving the loader to search everywhere but beside it);
#   - two versions of one library in a target folder fail, naming both;
#   - a versioned library outside a target folder fails, as an unversioned one does;
#   - a suffix that is not a version ("libhi.so.2x") is not taken for the library.
# Linux only, as the version suffix is ELF's.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-versioned-ffi.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/lo.c" <<'EOF'
int lo_value(void) {
  return 40;
}
EOF

cat > "${WORK}/hi.c" <<'EOF'
int lo_value(void);

int hi_value(void) {
  return lo_value() + 2;
}
EOF

# Build "source" as "file" (also its SONAME) in "pkg"'s host folder, hardened
# as lib.sh's add_ffi does, linked against any further "libs".
build_versioned() {
    local dir="$1" pkg="$2" file="$3" source="$4"
    shift 4
    mkdir -p "${dir}/ffi/${pkg}/${HOST_FFI}"
    cc -shared -fPIC -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -Wl,-z,relro,-z,now \
        -Wl,-soname,"${file}" -o "${dir}/ffi/${pkg}/${HOST_FFI}/${file}" "${source}" "$@"
}

# A project with both packages, its program calling "hi" alone.
make_project() {
    local dir="$1"
    new_project "${dir}"
    local lo="${dir}/ffi/lo/${HOST_FFI}/liblo.so.1.2.3"
    build_versioned "${dir}" lo liblo.so.1.2.3 "${WORK}/lo.c"
    build_versioned "${dir}" hi libhi.so.2 "${WORK}/hi.c" "${lo}"
    cat > "${dir}/ffi/lo/stub.spp" <<'EOF'
use std::cffi::CInt

!ffi(symbol="lo_value")
fun lo_value() -> CInt { }
EOF
    cat > "${dir}/ffi/hi/stub.spp" <<'EOF'
use std::cffi::CInt

!ffi(symbol="hi_value")
fun hi_value() -> CInt { }
EOF
    cat > "${dir}/src/main.spp" <<'EOF'
fun main() -> Void {
  std::console::println("{}", hi::hi_value())
}
EOF
}

# A build that must fail with "message" in its log.
check_refused() {
    local name="$1" dir="$2" message="$3"
    if (cd "${dir}" && "${SPP}" build -m dev > "${dir}/build.log" 2>&1); then
        fail "${name}: the build succeeded" "${dir}/build.log"
        return
    fi
    if tr '\r' '\n' < "${dir}/build.log" | grep -qF "${message}"; then
        ok "${name}: the build failed, saying ${message}"
    else
        fail "${name}: the build failed without saying ${message}" "${dir}/build.log"
    fi
}

check_runs() {
    local name="versioned, one needing another"
    local dir="${WORK}/runs/versioned"
    make_project "${dir}"
    build_project "${name}" "${dir}" dev || { failures=$((failures + 1)); return; }

    local staged
    staged="$(dirname "${EXE}")/lib"
    if [[ ! -f "${staged}/libhi.so.2" || ! -f "${staged}/liblo.so.1.2.3" ]]; then
        fail "${name}: not staged under their versioned names" <(ls "${staged}")
        return
    fi
    local needed
    needed="$(readelf -d "${EXE}" | grep NEEDED)"
    if ! grep -qF "[libhi.so.2]" <<< "${needed}" || ! grep -qF "[liblo.so.1.2.3]" <<< "${needed}"; then
        fail "${name}: the executable does not need both" <(echo "${needed}")
        return
    fi

    # No LD_LIBRARY_PATH: the loader must find both through the runpath alone.
    RC=0
    env -u LD_LIBRARY_PATH timeout --kill-after=5 60 "${EXE}" > "${dir}/run.log" 2>&1 || RC=$?
    if [[ ${RC} -eq 0 ]] && grep -qx "42" "${dir}/run.log"; then
        ok "${name}: staged versioned, both needed, runs"
    else
        fail "${name}: expected \"42\" and exit 0 (exit ${RC})" "${dir}/run.log"
    fi
}

check_two_versions() {
    local dir="${WORK}/two/versioned"
    make_project "${dir}"
    cp "${dir}/ffi/hi/${HOST_FFI}/libhi.so.2" "${dir}/ffi/hi/${HOST_FFI}/libhi.so.3"
    check_refused "two versions" "${dir}" "has more than one 'libhi.so'"
}

check_misplaced() {
    local dir="${WORK}/misplaced/versioned"
    make_project "${dir}"
    cp "${dir}/ffi/hi/${HOST_FFI}/libhi.so.2" "${dir}/ffi/hi/libhi.so.2"
    check_refused "versioned, outside a target folder" "${dir}" "'ffi/hi/libhi.so.2' is not in a target folder"
}

check_not_a_version() {
    local dir="${WORK}/suffix/versioned"
    make_project "${dir}"
    mv "${dir}/ffi/hi/${HOST_FFI}/libhi.so.2" "${dir}/ffi/hi/${HOST_FFI}/libhi.so.2x"
    check_refused "suffix that is not a version" "${dir}" "has no 'libhi.so'"
}

check_runs
check_two_versions
check_misplaced
check_not_a_version

finish
