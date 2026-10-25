---
name: cross-run-blockers-m68k-sparc64
description: "Why m68k and sparc64 cross-codegen entries are emit-only (no qemu run) — LLVM m68k abs eh_frame relocs; SPARC64 RWX PLT"
updated: 2026-10-24
---

As of 2026-10-24, 12 Linux cross targets link + run under qemu in `_cross_codegen.yaml`; m68k and sparc64 were left emit-only.

- **m68k**: LLVM's m68k backend writes absolute `R_68K_32` relocations into `.eh_frame` (not PC-relative), so the PIE link fails under `-z text` ("read-only segment has dynamic relocations"). Compiler-side fix needed.
- **sparc64**: GNU ld's SPARC64 PLT is writable+executable by ABI (the dynamic linker patches it; no secure-PLT). Both sppc's `libsppc.so` and spp's own PIE get an RWE segment, which spp's W^X link checks reject. Needs a policy decision (exemption or stay emit-only), not a CI fix.

**Why:** these look like CI breakage but are target ABI/backend limits.
**How to apply:** don't re-add `qemu:` to these entries until the above is resolved. Local harness gotcha: PyYAML (YAML 1.1) parses a matrix key `off` as boolean False; GitHub (YAML 1.2) doesn't. See `ci-pipeline-refactor-plan`.
