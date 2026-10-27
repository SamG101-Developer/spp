#!/usr/bin/env bash

# End-to-end check that "[memory.map] deny_wx" denies the process writable-
# and-executable memory (MDWE) and nothing more. Builds a scratch project
# whose ffi function tries to make a read-write page executable and asks a
# forked child for its own MDWE state, and asserts, at dev and rel:
#   - with "deny_wx = true" the page cannot gain exec (EACCES, 13);
#   - the child does not inherit the policy, so a child that JITs still can;
#   - with "deny_wx = false" the page gains exec, so the policy, and not
#     something else, is what refused it;
# and that "deny_wx = true" under the default floor fails the build, naming
# both keys.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-deny-wx.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/wx.c" <<'EOF'
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define PR_GET_MDWE_ 66

void wx_report(void) {
  void *const page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  const int gain = mprotect(page, 4096, PROT_READ | PROT_EXEC) == 0 ? 0 : errno;
  munmap(page, 4096);

  const pid_t pid = fork();
  if (pid == 0) { _exit(prctl(PR_GET_MDWE_, 0L, 0L, 0L, 0L) & 0x7f); }
  int status = 0;
  waitpid(pid, &status, 0);

  printf("gain=%d child=%d\n", gain, WEXITSTATUS(status));
  fflush(stdout);
}
EOF

make_project() {
    local dir="$1" deny="$2" floor="${3-6.6}"
    new_project "${dir}" "
[memory.map]
deny_wx = ${deny}
${floor:+
[target.floor]
linux = \"${floor}\"}"

    add_ffi "${dir}" wx "${WORK}/wx.c" <<'EOF'
use std::cffi::CVoid

!ffi(symbol="wx_report")
fun wx_report() -> CVoid { }
EOF

    cat > "${dir}/src/main.spp" <<'EOF'
fun main() -> Void {
  wx::wx_report()
}
EOF
}

# One case: build, run, and compare the report with what was expected.
check() {
    local mode="$1" deny="$2" expect="$3"
    local name="${mode}, deny_wx=${deny}"
    local dir="${WORK}/${mode}-${deny}/wx"
    make_project "${dir}" "${deny}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    if [[ ${RC} -eq 0 ]] && grep -qx "${expect}" "${dir}/run.log"; then
        ok "${name}: ${expect}"
    else
        fail "${name}: expected \"${expect}\" and exit 0 (exit ${RC})" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" true "gain=13 child=0"
    check "${mode}" false "gain=0 child=0"
done

# Asked for explicitly, below its kernel: a build error, not a quiet downgrade.
dir="${WORK}/floor/wx"
make_project "${dir}" true ""
if (cd "${dir}" && "${SPP}" build -m dev > "${dir}/build.log" 2>&1); then
    fail "deny_wx=true under the default floor: the build succeeded" "${dir}/build.log"
elif tr '\r' '\n' < "${dir}/build.log" | grep -qF "'deny_wx = true' in [memory.map] cannot be applied: needs Linux 6.6, above the floor 4.18; raise 'linux' in [target.floor]"; then
    ok "deny_wx=true under the default floor: the build failed, naming both keys"
else
    fail "deny_wx=true under the default floor: the build failed without naming both keys" "${dir}/build.log"
fi

finish
