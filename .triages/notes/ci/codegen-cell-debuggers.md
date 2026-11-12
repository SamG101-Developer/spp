---
name: codegen-cell-debuggers
description: The codegen cell installs lldb-23, LLVM's DWARF tools and a pinned gdb itself; a missing tool fails there (SPP_CI_REQUIRE_TOOLS)
updated: 2026-11-12
---

The test job has no toolchain (`setup-runtime` only fetches the built binaries), and the `ubuntu-24.04` image has no
gdb and no lldb. The debug-info checks (`check-debug-info`, `-formatters`, `-scopes`, `-moves`) need gdb, `lldb-23`
(with `python3-lldb-23`) and `llvm-dwarfdump-23`/`llvm-symbolizer-23`, so `_test.yaml` installs them in cells that
hold `codegen`: `fetch-llvm-sh.sh` then `install-debuggers-linux.sh` (llvm.sh at `[pin.llvm-lib]`, plus `llvm-23`,
`python3-lldb-23`, and gdb's GMP/MPFR/Python dev packages, which a cache hit still needs at run time), and gdb built
from `[pin.gdb]` by `install-gdb-linux.sh` into `SPP_CI_GDB_PREFIX`, cached like valgrind. Harden Runner allows sudo
in those cells. gdb is configured with `--with-python=/usr/bin/python3`, not setup-python's, as it links libpython.

The checks skip a missing tool locally (`have_tool`/`need_tool` in `tests/codegen/lib.sh`), and the codegen step sets
`SPP_CI_REQUIRE_TOOLS=1`, which makes a missing one fail instead. Before this, every debugger part skipped in CI and
passed having checked nothing; `install-llvm-linux.sh` even purges lldb (still right for the build job, whose Clang
step's lldb blocks the LLVM release's).

**Why:** a skipped check reports success, so CI showed green debug checks that ran no debugger.

**How to apply:** a new check that needs a tool the image lacks calls `need_tool`/`have_tool`, never a bare
`command -v ... || skip`, and the tool is installed in the codegen cell. A cross-compiler skip (`require_aarch64_cross`)
stays a skip: those lanes do not have one by design. gdb's pin refreshes with `refresh-pins.sh` like valgrind's.
Related: [[package-installs-retry]], [[codegen-checks-suite]].
