---
name: ci-gcc-from-homebrew
description: "Linux CI gets GCC from Homebrew (not the PPA), the Clang lanes their libstdc++ too (a clang.cfg); needs brew binutils; brew specs inject rpath + loader"
updated: 2026-11-18
---

Since 2026-09-18, `.github/scripts/setup/install-gcc-linux.sh` installs GCC from Homebrew (pre-installed on
both the x64 and arm64 Ubuntu runner images, off PATH) instead of `ppa:ubuntu-toolchain-r/test`, and `[pin.gcc]` in
`.github/dependencies.toml` is a full release (`16.2`), matched against `g++ -dumpfullversion` so a pin Homebrew cannot
serve fails the job. Workflows never name the version: `compile/export-env.sh` reads it from the pin (checked
2026-10-09). Since 2026-10-09 (phase 0 of
.triages/docs_closed/ci-pipeline-refactor.md) CI links libstdc++ statically via `SPP_STATIC_RUNTIME`, and the `LD_LIBRARY_PATH`
export is gone; the script instead fails if brew ships no `libstdc++.a`.

**Why:** that PPA only ever publishes dated trunk snapshots, and its newest gcc-16 for noble is from 2026-03-15 - months
before the fix for PR125768, the imported-incomplete-type modules ICE this project was hitting in `symbols.cpp`. Real
16.2.0 packaging exists only in Ubuntu 26.10 and Debian sid, neither of which has a runner image.

**How to apply:** Homebrew's GCC is configured against Homebrew's binutils, so it emits `.base64` and the runner's
assembler (2.42) rejects it - the script installs `binutils` too and exports `COMPILER_PATH` at it, which moves only the
compiler's own `as`/`ld`. If this path ever needs
abandoning, the known-green fallback is the PPA snapshot plus per-file `-Wno-uninitialized` on `symbols.cpp`, for the
reason in `gcc-source-tree-for-ice`.

**Homebrew's specs file (2026-10-09):** `brew install gcc` writes `<libgcc dir>/specs` (brew's
`install_steps/formula_actions.rb`) appending `--dynamic-linker <brew>/lib/ld.so -rpath <brew>/lib/gcc/current` to
`*link:` and a `*homebrew_rpath:` `-rpath <brew>/lib`. Every CI binary therefore had brew's loader + RUNPATH; on arm64
libz/libzstd resolved into brew. `install-gcc-linux.sh` now strips both blocks with awk (gcc rejects a run of blank
lines or an empty spec body, so the blank line after each block goes too), and `check-self-contained.sh` fails on a
non-`/lib*` interpreter or any RUNPATH.

**The Clang lanes too (2026-11-18):** `install-clang-linux.sh` runs `install-gcc-linux.sh` and points clang at that GCC
with `--gcc-install-dir`, written to `clang.cfg` and `clang++.cfg` in clang's resolved bin directory
(`/usr/lib/llvm-N/bin`; one beside the `/usr/bin` symlink is not read, checked locally), which every clang run reads,
the sibling libraries' builds included. It checks `-v`'s "Selected GCC installation", and exports
`SPP_CI_GCC_INSTALL_DIR`, which `configure.sh` adds to the C/C++ flags so `compile_commands.json` (clang-tidy's own
driver) carries it. Homebrew keeps `libstdc++` and `libgcc_s` beside the GCC (where `-print-file-name=libstdc++.a`
points), not in the install dir, so a plain link failed CMake's first check ("cannot find -lgcc_s"): the config also
has `-L` that dir, `-static-libgcc` and `-static-libstdc++` (the runner's `libstdc++.so.6` is older than GCC 16's),
inside `--start-no-unused-arguments`/`--end-no-unused-arguments`, or every compile-only call warns and `-Werror` fails
it. Homebrew also keeps libstdc++'s headers at `<cellar>/include/c++/16`, not where clang looks beside a GCC
("'stdexcept' file not found"): the config takes the C++ include dirs Homebrew's `g++ -E -v` reports, as `-cxx-isystem`,
with `-nostdinc++` so no other GCC's headers mix in, and `-B<runtime dir>` so `-print-file-name` (CMake's `import std`
asks for `libstdc++.modules.json`) finds the runtime's files, which `-L` does not. `SPP_CI_CLANG_CXX_FLAGS` carries the
C++ ones into `configure.sh`'s C++ flags. The script then builds and runs a C and a C++ program (an exception thrown and caught) so a broken link fails in
setup. Before, they installed `g++-16` from the PPA: the March trunk snapshot's libstdc++, and a
Launchpad 503 outage that day failed Linux Clang and Clang-Tidy after `retry`'s three tries.
