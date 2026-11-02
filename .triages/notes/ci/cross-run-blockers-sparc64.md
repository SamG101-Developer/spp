---
name: cross-run-blockers-sparc64
description: "Why sparc64 is the one Linux cross-codegen entry that is emit-only (its ABI's PLT is writable and executable), and why m68k's unit tests cannot link (LLVM's 16-bit PIC reach)"
updated: 2026-11-01
---

As of 2026-10-26, 15 Linux cross targets link and run under qemu in `_cross_codegen.yaml`, and musl links and runs in
Alpine (`run-in: "alpine"`, `install-alpine-musl-linux.sh`); sparc64 alone is emit-only. aarch64_be has no Ubuntu
gcc: `toolchain: "arm-gnu"` fetches Arm's release (`[pin.arm-gnu]`), whose sysroot qemu reads via `SPP_CI_QEMU_SYSROOT`.

- **sparc64**: GNU ld's SPARC64 PLT is writable+executable by ABI (the dynamic linker patches it; no secure-PLT). Both sppc's `libsppc.so` and spp's own PIE get an RWE segment, which spp's W^X link checks reject. Needs a policy decision (exemption or stay emit-only), not a CI fix.
- Fixed, for reference: **m68k** (LLVM writes absolute `R_68K_32` relocs into `.eh_frame`, failing `-z text`) runs because spp emits no unwind tables there and the entry turns `ffi_unwind_guard` off. **32-bit powerpc** hit the same RWE-PLT check through LLVM's default BSS PLT; spp now sets `+secure-plt` on ppc32 Linux, as Debian's gcc (`--enable-secureplt`) does.
- **m68k unit tests** (2026-11-01): the hello program links and runs, but `spp test --all-libs` does not link: "relocation truncated to fit: R_68K_PC16 / R_68K_GOT16". LLVM's m68k PIC code reaches calls and the GOT through 16-bit displacements, so `.text` past about 32 KB overflows. `-code-model=large` still emits `PLT16` and `GOT16`, and `-mcpu=M68020` doesn't help. Only non-PIC with `-code-model=large` links (reproduced with a 200 KB generated `.ll`), and under spp's `-z text` only with `-no-pie`, which gives up PIE on m68k. This limits any m68k program, not only the tests. CI's real test IR, built that way by hand with `llc-23` and linked against `libsppc.so` with spp's other flags, links with no text relocations, and then 451/554 tests pass run alone under qemu (114 fail at `llc -O0`). The failures look like m68k backend miscompiles, not PIC: one crash follows `move.w %d5, %d2` then `move.l %d2, %a0`, where the top half looks stale, and the PIC compile of the same IR has the same sequence. Not yet confirmed as the cause.

**Why:** these look like CI breakage but are target ABI/backend limits.
**How to apply:** don't add `qemu:` to sparc64 until the above is decided. A new 32-bit target failing with "segment both writable and executable": check for a secure-PLT-style target feature before blaming the linker. Local harness gotcha: PyYAML (YAML 1.1) parses a matrix key `off` as boolean False; GitHub (YAML 1.2) doesn't. See `ci-pipeline-refactor-plan`.
