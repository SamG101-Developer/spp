#!/usr/bin/env bash

# End-to-end check that the stack canary fires. Builds a scratch project whose
# ffi function writes past a "&mut U8" it was lent - the one way left to smash
# an s++ frame - and asserts, at dev and rel:
#   - an overflow is caught ("stack smashing detected", SIGABRT);
#   - an in-bounds write is not;
#   - with "[memory.stack] protect = false" the overflow is not caught, so the
#     canary, and not something else, is what caught it.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-stack-protector.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

# One byte past the lent U8, into the canary, which sits directly above it.
# Any further runs off the top of the unsafe-stack mapping, since the victim's
# is the only unsafe frame, and faults before the canary is checked.
readonly OVERFLOW=9

cat > "${WORK}/smash.c" <<'EOF'
#include <stdint.h>
void smash(uint8_t *p, uint64_t n) { for (uint64_t i = 0; i < n; ++i) { p[i] = 0x41; } }
EOF

make_project() {
    local dir="$1" protect="$2" count="$3"
    # The probe writes through a pointer of unknown size, which no fortification
    # can check, so its library imports no "__*_chk" and would read as built
    # without it; that is check-fortified-ffi.sh's business, not this check's.
    new_project "${dir}" "
[memory.stack]
protect = ${protect}

[binary.link]
fortified_ffi = false"

    add_ffi "${dir}" smash "${WORK}/smash.c" <<'EOF'
use std::cffi::CULong
use std::cffi::CVoid

!ffi(symbol="smash")
fun smash(p: &mut U8, n: CULong) -> CVoid { }
EOF

    cat > "${dir}/src/main.spp" <<EOF
fun victim() -> Void {
  let mut x = 0_u8
  smash::smash(&mut x, ${count}_u64)
}

fun main() -> Void {
  victim()
}
EOF
}

# One case: build, run, and compare whether the canary fired with what was
# expected.
check() {
    local mode="$1" protect="$2" count="$3" expect="$4"
    local name="${mode}, protect=${protect}, ${count} byte(s)"
    local dir="${WORK}/${mode}-${protect}-${count}/canary"
    make_project "${dir}" "${protect}" "${count}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    local fired=no
    grep -q "stack smashing detected" "${dir}/run.log" && fired=yes
    if [[ "${expect}" == "no" && "${protect}" == "true" && ${RC} -ne 0 ]]; then
        fail "${name}: an in-bounds write should exit 0 (exit ${RC})" "${dir}/run.log"
    elif [[ "${fired}" == "${expect}" ]]; then
        ok "${name}: canary fired=${fired} (exit ${RC})"
    else
        fail "${name}: canary fired=${fired}, expected ${expect} (exit ${RC})" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" true "${OVERFLOW}" yes
    check "${mode}" true 1 no
    check "${mode}" false "${OVERFLOW}" no
done

finish
