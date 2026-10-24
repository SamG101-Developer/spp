#!/usr/bin/env bash

# End-to-end check that "[binary.link] segments_ffi" fails the link against an
# ffi library whose code shares a page with data, and nothing else. Builds a
# scratch project whose ffi library is linked three ways, and asserts:
#   - with "-z separate-code" the program links and runs;
#   - with "-z noseparate-code" the build fails naming the fault, and the key;
#   - with "-z noseparate-code" and "segments_ffi = false", it links and runs.
# The check is made at link time, the same at dev and rel, so rel alone runs.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-segments-ffi.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/greet.c" <<'EOF'
#include <stdio.h>

void probe_greet(void) {
  puts("hello");
  fflush(stdout);
}
EOF

# Full RELRO always, so only the segment layout differs.
RELRO="-Wl,-z,relro,-z,now"

# A project calling "probe_greet", its library relinked with "ldflags", and
# "config" after its [vcs] section.
make_project() {
    local dir="$1" ldflags="$2" config="${3:-}"
    new_project "${dir}" "${config}"
    add_ffi "${dir}" probe "${WORK}/greet.c" <<'EOF'
use std::cffi::CVoid

!ffi(symbol="probe_greet")
fun probe_greet() -> CVoid { }
EOF
    # shellcheck disable=SC2086 # the flags are words
    cc -shared -fPIC -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 ${RELRO} ${ldflags} \
        -o "${dir}/ffi/probe/${HOST_FFI}/libprobe.so" "${WORK}/greet.c"
    cat > "${dir}/src/main.spp" <<'EOF'
fun main() -> Void {
  probe::probe_greet()
}
EOF
}

# A build that must link and print "hello".
check_runs() {
    local name="$1" ldflags="$2" config="${3:-}"
    local dir="${WORK}/${name//[ ,]/-}/segments"
    make_project "${dir}" "${ldflags}" "${config}"
    build_project "${name}" "${dir}" rel || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"
    if [[ ${RC} -eq 0 ]] && grep -qx "hello" "${dir}/run.log"; then
        ok "${name}: links and runs"
    else
        fail "${name}: expected \"hello\" and exit 0 (exit ${RC})" "${dir}/run.log"
    fi
}

# A build that must fail, naming "fault" and the key.
check_refused() {
    local name="$1" ldflags="$2" fault="$3"
    local dir="${WORK}/${name//[ ,]/-}/segments"
    make_project "${dir}" "${ldflags}"
    if (cd "${dir}" && "${SPP}" build -m rel > "${dir}/build.log" 2>&1); then
        fail "${name}: the build succeeded" "${dir}/build.log"
        return
    fi
    if tr '\r' '\n' < "${dir}/build.log" | grep -qF "was linked with ${fault}." &&
        tr '\r' '\n' < "${dir}/build.log" | grep -qF "segments_ffi = false"; then
        ok "${name}: the build failed, naming ${fault} and the key"
    else
        fail "${name}: the build failed without naming ${fault} and the key" "${dir}/build.log"
    fi
}

check_runs "separate code" "-Wl,-z,separate-code"
check_refused "shared code page" "-Wl,-z,noseparate-code" "code sharing a page with the ELF header"
check_runs "shared code page, check off" "-Wl,-z,noseparate-code" "
[binary.link]
segments_ffi = false"

finish
