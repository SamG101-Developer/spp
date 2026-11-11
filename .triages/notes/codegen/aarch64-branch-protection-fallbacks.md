---
name: aarch64-branch-protection-fallbacks
description: "bti and pac_ret act only on AArch64 ELF; elsewhere cfi and split stand in (off needs them off); bti left out where the toolchain's start files lack BTI"
updated: 2026-11-11
---

`[binary.codegen] bti` (S10.1) and `pac_ret` (S10.3) emit anything only where AArch64 ELF enforces it
(`codegen::DoesTargetSupportBti`, `DoesTargetSupportPacRet`, in `llvm_support`), as clang's `-mbranch-protection=bti+pac-ret` does
(`ApplyBranchTargets`, `ApplyReturnSigning` in `llvm_cfi.cpp`, the last hardening passes): function attributes plus
a module flag each. LLVM pads only what an indirect branch can reach (`bti c` on functions called indirectly, most
being internal after LTO; `bti j` on the hardened jump tables' targets) and signs only functions that save their
return address; a signing function's `paciasp` is its landing pad. **Elsewhere each key rests on another:** `bti` on
KCFI (`cfi`), `pac_ret` on the split stack (`[memory.stack] split`, which keeps what could overflow away from return
addresses, and fails the build itself where the target has none). So `cfi = false` needs `bti = false`, and
`split = false` needs `pac_ret = false`, off AArch64 ELF: every cross-codegen row, CI lane and check script that turns
one off turns the other off beside it.

The linker keeps the executable's BTI property only if every object linked has it, and drops it silently otherwise.
**Some toolchains' own start files lack it** (Ubuntu 24.04's glibc `crti.o`/`crtn.o`; Arm's aarch64_be gcc's
`crtbeginS.o`/`crtendS.o` and `libgcc.a` atomics), while Debian's all have it. So before the passes,
`StartFileWithoutBti` (`compiler_link.cpp`) asks the link's driver for its start files (`-print-file-name=`) and reads
their properties: one without it makes `bti` **left out** when on by default (`features::LeaveOut`; the record reads
`left out (the toolchain's crti.o has no BTI property)`, no pads are emitted), and **refused** when the project set
`bti = true` itself (`features::IsExplicit`). After linking, `UnguardedBranchTargets` (`llvm_artefact.cpp`) still reads
the property back and fails the build if anything else dropped it. A shared library does not count, as each object is
guarded on its own. The PAC property is not checked: nothing enforces it, as signing is in the code.

**Why:** a measure nothing enforces is a silent no-op, which S0.4 forbids; KCFI already confines an indirect call to a
function of its own type, stricter than any landing pad.
**How to apply:** turning `cfi` or `split` off means turning `bti` or `pac_ret` off with it. Check scripts testing
one measure turn the other off (a branch past a signing prologue is caught by either). `tests/codegen/check-bti.sh`
and `check-pac-ret.sh` build for aarch64 and run under `qemu-aarch64 -cpu max` (BTI and FEAT_FPAC enforced); they
need an all-targets spp and the cross gcc (`require_aarch64_cross` in `lib.sh`).
Related: [[kcfi-bundles-not-metadata]], [[hardened-jump-tables]], [[hardening-needs-feature-gate]].
