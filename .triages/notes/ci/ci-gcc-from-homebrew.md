---
name: ci-gcc-from-homebrew
description: "Linux CI gets GCC from Homebrew (not the PPA); needs brew binutils; brew specs inject rpath + loader"
updated: 2026-10-09
---

Since 2026-09-18, `.github/scripts/setup/install-gcc-linux.sh` installs GCC from Homebrew (pre-installed on
both the x64 and arm64 Ubuntu runner images, off PATH) instead of `ppa:ubuntu-toolchain-r/test`, and `[pin.gcc]` in
`.github/dependencies.toml` is a full release (`16.2`), matched against `g++ -dumpfullversion` so a pin Homebrew cannot
serve fails the job. Workflows never name the version: `compile/export-env.sh` reads it from the pin (checked
2026-10-09). The clang jobs still take their libstdc++ from the PPA. Since 2026-10-09 (phase 0 of
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
