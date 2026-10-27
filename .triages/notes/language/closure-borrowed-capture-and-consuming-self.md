---
name: closure-borrowed-capture-and-consuming-self
description: "Two memory bugs found via the socket example (2026-09-22): borrowed captures of a borrow were pointer-to-pointer (stack smash); consuming self through a borrow was unchecked (double free)"
---

- **`caps &mut out` where `out` is already `&mut T`** stored the address of the local holding the pointer, so
  `out@ = v` in the closure overwrote that local (stack smash, write lost). The env now uses `codegen::LlvmAddrOf`,
  which re-borrows. This emptied every `BufRead::read_line` result. Runtime test in std `tst/std/result.spp` - the
  gtest suite cannot see it (the IR is valid).
- **Calling a `self`-by-value method through a borrow** (`a.take()` with `a: &mut A`, or on the `&mut` a chaining
  `cor` returns) was accepted and double-freed. Now E7 in `FunctionCallArgumentGroupAst::Stage8_CheckMemory` (self arg,
  by-value self param, borrowed non-Copy receiver). It found real std sites: `BigUInt::rem_assign`
  (`self / x`), `Path::div_assign` (`self@ = self.join(..)`) - fixed with `.clone()`. `StrBuilder` became a by-value
  builder (`append(mut self, ..) -> Self`) plus `Clone`.
- Mirror std fixes into `tests/test_outputs/vcs/std` too, or every gtest fails on std itself.

Also found and fixed the same day: `Vec::new_filled` was a stub that only set the length (now in the `T: Clone`
sups, built on `resize`); a `case` passed straight as an argument emitted a null LLVM operand, because
`CaseExpressionAst` only built a phi under an assignment target - it now yields whenever it has an `else` and a
non-valueless type (asking `InferType` with `IgnoreMissingElseBranchForInference`, or std's else-less cases raise E13).

**Loops had the same value-in-argument gap (fixed 2026-09-22), plus two more:** `f(loop true { .. exit 5 })` crashed
the compiler. (1) The loop only built its exit phi under an assignment target - dropped, as for `case`. (2)
`LoopConditionalExpressionAst::Clone` did not carry `_LoopExitTypeInfo`, and the argument copy overload
resolution analysed was the only one that had it, so the codegen node read as a `!` loop - now copied. (3) The `exit`
codegen re-inferred its expression's type to test for `!`, which crashed intermittently (pointer-order dependent) on
this path; now recorded at Stage 7 (`_ExprIsNever`, carried by Clone). General lesson: analysis results needed in
codegen must be cached at Stage 7 AND copied by Clone - [[ast-clone-carries-cached-analysis]].
