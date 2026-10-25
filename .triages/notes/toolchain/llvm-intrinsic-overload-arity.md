---
name: llvm-intrinsic-overload-arity
description: "An intrinsic overloaded on more than one type crashes in LLVM's IR printer, far from where it was built"
---

`SimpleBinaryIntrinsicCall` in `llvm_fn_impls.cpp` passes **one** type for both operands and for the intrinsic's overload list. That is right for intrinsics overloaded on a single type, and wrong for any that are not.

`llvm.powi` is the case that bit: `float_powi(base: T, exponent: S32)` is overloaded on *both* the float and the integer exponent, and its operands are different types. Going through the shared helper built a function taking two floats and asked `getOrInsertDeclaration` for a half-specified overload. `StdIntrinsicsFpowi` now builds it bespoke with `{ty, i32_ty}` for both the signature and the overload list.

**Why it was hard to find:** the malformed declaration did not fault where it was created. The module died later inside `MDFieldPrinter::printBool` while the IR was being *printed*, and the progress line still read "Resolving comptime constants", so it looked like a comptime-resolution crash. A `gdb` backtrace named the LLVM printer frame and then hit the usual `#1 ?? ()` truncation — that one frame was still what cracked it.

**How to apply:** when the compiler dies inside an LLVM `AsmWriter`/`MDFieldPrinter` frame, suspect a malformed declaration built earlier, not the stage the progress bar names. Check any intrinsic whose S++ signature has operands of differing types against the arity of its LLVM overload list. Covered by `std::tst::num::sized_floating_point::test_powers_of_two_are_exact`, which exercises both `pow` overloads on purpose. Related: [[asan-build-for-llvm-crashes]].
