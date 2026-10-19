#!/usr/bin/env bash

# End-to-end check that an exception from foreign code cannot unwind through
# s++ frames. Builds a scratch project against a C++ library whose "guarded"
# calls an s++ closure inside a try/catch, and whose "boom" throws, and asserts,
# at dev and rel:
#   - "boom" thrown under "guarded" (a C++ handler above the s++ closure) fails
#     the "unwound into s++" check naming "boom", rather than being caught
#     after skipping the closure's frame;
#   - a callback that does not throw returns through "guarded" and exits 0;
#   - "boom" thrown with no handler anywhere still aborts (through
#     "std::terminate": C's personality only runs cleanups, so the runtime
#     gives up before reaching the guard).
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-ffi-unwind.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/thrower.cpp" <<'EOF'
#include <cstdio>
#include <stdexcept>

typedef struct {
  void (*fn)(void *);
  void *env;
} sppc_closure;

extern "C" void boom(void) { throw std::runtime_error("boom"); }

extern "C" void calm(void) { }

extern "C" void guarded(sppc_closure cb) {
  try {
    cb.fn(cb.env);
    std::puts("returned");
  }
  catch (std::exception const &) {
    std::puts("caught");
  }
  std::fflush(stdout);
}
EOF

make_project() {
    local dir="$1" body="$2"
    new_project "${dir}"

    add_ffi "${dir}" thrower "${WORK}/thrower.cpp" <<'EOF'
use std::cffi::CClosure
use std::cffi::CVoid

!ffi(symbol="boom")
fun boom() -> CVoid { }

!ffi(symbol="calm")
fun calm() -> CVoid { }

!ffi(symbol="guarded")
fun guarded(cb: CClosure[(), Void]) -> CVoid { }
EOF

    cat > "${dir}/src/main.spp" <<EOF
fun main() -> Void {
  ${body}
  std::console::println("after")
}
EOF
}

# One case: build, run, and compare the exit code and output with what was
# expected; "forbid" is a whole line that must not appear.
check() {
    local mode="$1" name="$2" body="$3" rc_want="$4" expect="$5" forbid="$6"
    name="${mode}, ${name}"
    local dir="${WORK}/${mode}-${RANDOM}/unwind"
    make_project "${dir}" "${body}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    local why=""
    if [[ ${RC} -ne ${rc_want} ]]; then
        why="exit ${RC}, expected ${rc_want}"
    elif ! grep -qF "${expect}" "${dir}/run.log"; then
        why="no \"${expect}\" in the output"
    elif [[ -n "${forbid}" ]] && grep -qxF "${forbid}" "${dir}/run.log"; then
        why="\"${forbid}\" in the output"
    fi

    if [[ -z "${why}" ]]; then
        ok "${name} (exit ${RC})"
    else
        fail "${name}: ${why}" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" "thrown under a C++ handler" \
        "thrower::guarded(std::cffi::CClosure[(), Void]::from(() -> Void { thrower::boom() }))" 134 \
        "[sppc] check failed: an exception unwound into s++ from 'boom'" "caught"
    check "${mode}" "callback that does not throw" \
        "thrower::guarded(std::cffi::CClosure[(), Void]::from(() -> Void { thrower::calm() }))" 0 \
        "after" ""
    check "${mode}" "thrown with no handler" "thrower::boom()" 134 "terminate" "after"
done

finish
