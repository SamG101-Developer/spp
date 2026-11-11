---
name: spp-full-width-shift-is-poison
description: "A shift by >= the operand's own bit width silently did nothing; now clamped in ApplyBinOp (0, or all sign bits for AShr)"
updated: 2026-11-09
---

`x >>= 32_u32` on a `U32` used to be a **no-op**, not a zero. `llvm_fn_impls.cpp` emitted a bare
`CreateLShr`/`CreateShl`; `lshr i32 %x, 32` is poison in LLVM, and x86 masks a variable shift count to
the low 5/6 bits, so the count became 0 and the value came back unchanged.

That is what made `BigUInt::from` loop forever (`loop temp != zero { data.append(...); temp >>= 32 }`)
and OOM the machine — see [[spp-run-binary-memory-cap]].

Fixed 2026-08-31 in `EmitDefinedShift` (anon namespace of `llvm_fn_impls.cpp`), routed through
`ApplyBinOp` for both `Shl` and `LShr`: the distance is clamped to `width-1` before the shift and the
result is `select`ed to 0 when the distance is `>= width`. **Defined, not diagnosed** — unlike an
out-of-bounds index, a shift past the width has one obvious answer. `AShr` (2026-11-09, signed `>>`, see
[[signed-shr-is-arithmetic]]) shares the clamp but skips the `select`: shifting by `width-1` already
gives the past-the-width answer, all sign bits (0 or -1).

Also fixed alongside: `SimpleIntrinsicBinopAssign` sized the loaded/stored slot from `that` rather
than `this`. Harmless for every op except a shift, whose distance is separately typed
(`bit_shr_assign(&mut self, that: U32)`) — so `&mut U64 >>= U32` was loading and storing 32 bits of a
64-bit slot.

Watch for the same shape in `.spp`: `bit_shl_wrapping` (`sized_integer.spp`) and `bit_shr_wrapping`
(per signedness, `sized_integer_signed.spp` / `_unsigned.spp`) mask with `& (w - 1_u32)` and are safe;
anything shifting by a computed distance is not automatically.
