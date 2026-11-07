---
name: arm-narrow-atomics-sync-libcall-extension
description: "On pre-v6 ARM, LLVM lowers atomics to libgcc __sync_* calls and assumes a zero-extended result, but libgcc sign-extends: U8 > 127, U16 > 32767 and negative S8/S16 miscompare"
updated: 2026-11-01
---

`arm-unknown-linux-gnueabi` builds with the "generic" CPU (`llvm_target.cpp`), which has no `ldrex`, so LLVM 23 lowers every atomic to a libgcc `__sync_*_N` call. For 8 and 16 bits, its codegen treats the result as already zero-extended: `icmp eq i8 %v, -56` becomes `bl __sync_val_compare_and_swap_1; sub r0, r0, #200`, with no mask, and a `zext` emits nothing. libgcc (Debian gcc 14 here, CI's gcc 13 fails the same way) returns it sign-extended: the byte 200 comes back as `0xffffffc8`, and `__sync_fetch_and_add_1` and `__sync_lock_test_and_set_1` do the same.

So a narrow atomic whose top bit is set compares wrong. It shows as `std::tst::threading::atomic::test_an_atomic_of_a_narrower_width` aborting with "the value at U8". 32-bit atomics are not affected, and neither are armv6k and later, which inline `ldrex`/`strex`. `-mcpu=arm926ej-s` (v5) still fails.

Reproduced without spp: a `.ll` doing these loads through `llc-23`, `arm-linux-gnueabi-gcc -pie` and `qemu-arm`. A `sext` is safe because LLVM does emit the shifts for it.

Candidate fixes, tried only on that `.ll`: an empty inline asm on the loaded value (`asm "", "=r,0"`) hides the assumption and survives `opt -O2`; a v6k CPU avoids the calls but drops ARMv5 hardware from the target.

**Why:** it is a real miscompile on this target, not a test problem. The `spp` side emits a correct `load atomic i8`.

**How to apply:** an ARM failure on a narrow atomic only for high values: look here first. This matters for every narrow atomic operation that compares or widens its result, `compex` too, not just `load`.
