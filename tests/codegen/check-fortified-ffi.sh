#!/usr/bin/env bash

# End-to-end check that "[binary.link] fortified_ffi" fails the link against an
# ffi library built without _FORTIFY_SOURCE, and nothing else. Builds a scratch
# project whose ffi library copies into a buffer of known size, and asserts:
#   - built with -O2 -D_FORTIFY_SOURCE=3 (so it imports "__memcpy_chk"), the
#     program links and runs;
#   - built without, the build fails naming the call and the key;
#   - built without, with "fortified_ffi = false", it links and runs;
#   - a library without a call to fortify links, however it was built.
# The check is made at link time, the same at dev and rel, so rel alone runs.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-fortified-ffi.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/copy.c" <<'EOF'
#include <stdio.h>
#include <string.h>

void probe_copy(void) {
  char buf[16];
  volatile size_t n = 5;
  memcpy(buf, "hello", n);
  buf[n] = '\0';
  puts(buf);
  fflush(stdout);
}
EOF

cat > "${WORK}/plain.c" <<'EOF'
#include <stdio.h>

void probe_copy(void) {
  puts("hello");
  fflush(stdout);
}
EOF

# A project calling "probe_copy" from "source", its library rebuilt with
# "cflags", and "config" after its [vcs] section.
make_project() {
    local dir="$1" source="$2" cflags="$3" config="${4:-}"
    new_project "${dir}" "${config}"
    add_ffi "${dir}" probe "${source}" <<'EOF'
use std::cffi::CVoid

!ffi(symbol="probe_copy")
fun probe_copy() -> CVoid { }
EOF
    # shellcheck disable=SC2086 # the flags are words
    cc -shared -fPIC ${cflags} -Wl,-z,relro,-z,now -o "${dir}/ffi/probe/${HOST_FFI}/libprobe.so" "${source}"
    cat > "${dir}/src/main.spp" <<'EOF'
fun main() -> Void {
  probe::probe_copy()
}
EOF
}

# A build that must link and print "hello".
check_runs() {
    local name="$1" source="$2" cflags="$3" config="${4:-}"
    local dir="${WORK}/${name// /-}/fortified"
    make_project "${dir}" "${source}" "${cflags}" "${config}"
    build_project "${name}" "${dir}" rel || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -eq 0 ]] && grep -qx "hello" "${dir}/run.log"; then
        ok "${name}: links and runs"
    else
        fail "${name}: expected \"hello\" and exit 0 (exit ${RC})" "${dir}/run.log"
    fi
}

# A build that must fail, saying "expect" (each line of it).
check_refused() {
    local name="$1" source="$2" cflags="$3"
    local dir="${WORK}/${name// /-}/fortified"
    make_project "${dir}" "${source}" "${cflags}"
    if (cd "${dir}" && "${SPP}" build -m rel > "${dir}/build.log" 2>&1); then
        fail "${name}: the build succeeded" "${dir}/build.log"
        return
    fi
    if tr '\r' '\n' < "${dir}/build.log" | grep -qF "calls 'memcpy' unchecked" &&
        tr '\r' '\n' < "${dir}/build.log" | grep -qF "fortified_ffi = false"; then
        ok "${name}: the build failed, naming the call and the key"
    else
        fail "${name}: the build failed without naming the call and the key" "${dir}/build.log"
    fi
}

readonly FORTIFIED="-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3"
readonly UNFORTIFIED="-O0 -U_FORTIFY_SOURCE"

# The fortified case only means something if fortification did reach the call.
# shellcheck disable=SC2086 # the flags are words
cc -shared -fPIC ${FORTIFIED} -o "${WORK}/libsanity.so" "${WORK}/copy.c"
if ! nm -D "${WORK}/libsanity.so" | grep -q " U __memcpy_chk"; then
    fail "the probe built with ${FORTIFIED} does not import __memcpy_chk, so this check proves nothing"
fi

check_runs "fortified library" "${WORK}/copy.c" "${FORTIFIED}"
check_refused "unfortified library" "${WORK}/copy.c" "${UNFORTIFIED}"
check_runs "unfortified library, check off" "${WORK}/copy.c" "${UNFORTIFIED}" "
[binary.link]
fortified_ffi = false"
check_runs "nothing to fortify" "${WORK}/plain.c" "${UNFORTIFIED}"

finish
