---
name: checked-arithmetic-is-default
description: "+, - and * abort (sppc_check_failed) on overflow in every profile; std code that wants wrapping must say so by name"
---

As of 2026-09-05, `+`, `-`, `*` and their `_assign` forms abort on overflow in **every** build profile (not Rust's
checked-in-dev / wrapping-in-release split). `intrinsics::add/sub/mul` are gone, replaced by signed/unsigned pairs
`sadd/uadd/ssub/usub/smul/umul`, and the `Add`/`Sub`/`Mul` sups moved out of `sized_integer.spp` into
`sized_integer_signed.spp` / `sized_integer_unsigned.spp` (mirroring how `sdiv`/`udiv` were already split). Codegen is
`EmitOverflowPair` + `EmitCheckedArith` in `llvm_fn_impls.cpp`; a failed check calls `sppc_check_failed` with a
check per operation (0x10 sadd … 0x15 umul), reported as e.g. "signed add overflowed", exit 134 (S3.8, 2026-10-17;
was a bare `llvm.ubsantrap` SIGILL). An overflow now ends with "[sppc] check failed", not "Illegal instruction".

**Why:** any algorithm that *relies* on wrapping now aborts. Two were found and fixed in std - `splitmix64` and
xoshiro's `next_u64` in `random.spp`, and the limb borrow in `BigUInt::sub_assign` - all rewritten to
`add_wrapping`/`sub_wrapping`/`mul_wrapping`.

**How to apply:** when new std code traps at runtime, first ask whether it meant to wrap; hashes, PRNGs, checksums and
limb-carry loops almost always do and should call the `*_wrapping` method by name. Everything else is a real bug the
check just found. The `_checked`/`_overflow` reporting methods work now too - they never could before, see
`llvm-intrinsics-unusable`.
