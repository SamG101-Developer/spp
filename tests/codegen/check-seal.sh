#!/usr/bin/env bash

# End-to-end check that "[memory.map] seal" seals what start-up loaded (mseal)
# and nothing that has to stay changeable. Builds a scratch project whose ffi
# function re-protects regions to the protection they already have - which
# only a seal refuses - and asserts, at dev and rel:
#   - with "seal = true" the executable's code and RELRO, and a library's code,
#     refuse it (EPERM, 1);
#   - the heap still grows and trims, as the writable segments beside it are
#     left unsealed;
#   - with "seal = false" every region accepts it, so the seal, and not
#     something else, is what refused it.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-seal.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

cat > "${WORK}/probe.c" <<'EOF'
#define _GNU_SOURCE
#include <errno.h>
#include <link.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>

static uintptr_t page(const uintptr_t addr) { return addr & ~(uintptr_t)(sysconf(_SC_PAGESIZE) - 1); }

static int reprotect(const uintptr_t addr, const int prot) {
  return mprotect((void*)page(addr), (size_t)sysconf(_SC_PAGESIZE), prot) == 0 ? 0 : errno;
}

static int exe_relro(struct dl_phdr_info *info, size_t size, void *out) {
  (void)size;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    if (info->dlpi_phdr[i].p_type == PT_GNU_RELRO) {
      *(uintptr_t*)out = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
    }
  }
  return 1; // the executable is first
}

void probe_report(void) {
  uintptr_t relro = 0;
  dl_iterate_phdr(exe_relro, &relro);
  const int exe_text = reprotect((uintptr_t)getauxval(AT_ENTRY), PROT_READ | PROT_EXEC);
  const int exe_ro = relro != 0 ? reprotect(relro, PROT_READ) : -1;
  const int lib_text = reprotect((uintptr_t)&probe_report, PROT_READ | PROT_EXEC);

  enum { N = 4096 };
  static void *blocks[N];
  for (int i = 0; i < N; ++i) { blocks[i] = malloc(1024); }
  for (int i = N - 1; i >= 0; --i) { free(blocks[i]); }
  malloc_trim(0);

  printf("exe_text=%d exe_relro=%d lib_text=%d heap=ok\n", exe_text, exe_ro, lib_text);
  fflush(stdout);
}
EOF

make_project() {
    local dir="$1" seal="$2"
    new_project "${dir}" "
[memory.map]
seal = ${seal}"

    add_ffi "${dir}" probe "${WORK}/probe.c" <<'EOF'
use std::cffi::CVoid

!ffi(symbol="probe_report")
fun probe_report() -> CVoid { }
EOF

    cat > "${dir}/src/main.spp" <<'EOF'
fun main() -> Void {
  probe::probe_report()
}
EOF
}

# One case: build, run, and compare the report with what was expected.
check() {
    local mode="$1" seal="$2" expect="$3"
    local name="${mode}, seal=${seal}"
    local dir="${WORK}/${mode}-${seal}/sealed"
    make_project "${dir}" "${seal}"
    build_project "${name}" "${dir}" "${mode}" || { failures=$((failures + 1)); return; }
    run_exe "${EXE}" "${dir}/run.log"

    if [[ ${RC} -eq 0 ]] && grep -qx "${expect}" "${dir}/run.log"; then
        ok "${name}: ${expect}"
    else
        fail "${name}: expected \"${expect}\" and exit 0 (exit ${RC})" "${dir}/run.log"
    fi
}

for mode in dev rel; do
    check "${mode}" true "exe_text=1 exe_relro=1 lib_text=1 heap=ok"
    check "${mode}" false "exe_text=0 exe_relro=0 lib_text=0 heap=ok"
done

finish
