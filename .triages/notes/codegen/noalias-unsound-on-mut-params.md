---
name: noalias-unsound-on-mut-params
description: "&mut params carry llvm noalias since 2026-10-13, sound only because the checker rejects every second way to the place (M7); history of the miscompile"
updated: 2026-11-10
---

**Now on (2026-10-13, `070a7d4c`).** `function_prototype_ast.cpp` puts `noalias` on every `&mut` parameter (beside
`readonly` on `&`, and `nonnull`, `noundef`, `dereferenceable` and, since 2026-11-10, `align` on both). It is sound
only because the memory checker refuses any call given two ways to one place where one is `&mut`: the memory-analysis
refactor's M7 (`.triages/docs_closed/memory-analysis-refactor.md`) closed every route found, pinned by
`test_mem_loans.cpp` and `test_mem_alias_routes.cpp`. A new way to alias a place that the checker misses is a
miscompile, not a missed optimisation. What follows is the history of why it was off.

**History.** `noalias` on `&mut` parameters was on once before, and caused a miscompile that surfaced as a segfault in an unrelated test far later in a single-process `spp test -l std` run — a test exited mid-body with no `[ OK ]` and control resumed at the top of `main`.

**Layer 1 is now fixed (2026-09-05).** `MemRegionOverlap` compares access paths, not `Ast::ToString()`: `RegionPath` reads an expression's `ExprParts()` and `IsRegionPathPrefix` walks the two step by step comparing `IdentifierAst::NameId()`. A subscript contributes no named step, so `v[mut i]` and `v[mut j]` are the same path and overlap. `FunctionCallArgumentGroupAst::Stage8_CheckMemory` also gained a branch for arguments with **no outermost symbol** — `v[mut i]` is rewritten to an `index_mut` call, so `FindVarSymbolOutermost` returns null and the `sym == nullptr` continue skipped it before any check ran. That branch takes the convention off the argument's *type* when none is spelled. It is deliberately gated on `sym == nullptr`: widening it to `arg->Conv == nullptr` wrongly counted the `self` of a `&mut self` method (whose type is already `&mut`) as a **new** borrow, and rejected `self -= (self / that.clone()) * that` in `biguint.spp`.

**Layer 2 is still open.** `v[mut i]` calls the `index_mut` *coroutine*, whose `&mut self` pin escapes into an unnamed generator handle; both recording sites are guarded by `if (handle and pins_required)`, so with no symbol to hang it on the pin is dropped. A tried-and-reverted fix (`EnclosingCallPinSink` on `CompilerMetaData`, folding handle-less pins into the enclosing call's borrow sets) worked generally but never fired for `Arr::swap`; cause never isolated, `MetaGuard` ruled out. Reverted 2026-09-01.

**Plan (2026-10-09):** `.triages/docs_closed/memory-analysis-refactor.md` M7. `noalias` is a property of the definition, not the call, so "noalias unless an index/slice on the same symbol is in play" (the maintainer) has to be enforced by the checker: two live borrows into one variable through an index/slice, one `&mut`, are an error. M1 replaced `RegionPath`/`MemRegionOverlap` with `regions::Place`/`Relate`; call steps (index/slice) must be opaque (M1.5).

**How to apply:** a green suite says nothing about `noalias` — it passes either way; soundness rests on the checker. Any change that adds a way to reach a place (a new borrow form, coroutine or closure capture, index or slice route) needs a `test_mem_alias_routes.cpp` case proving the checker rejects the second `&mut` way in. Layer 2 was closed by M7.2 (found done 2026-10-10: the index coroutine's `&mut self` loan is held by the binding receiving the borrow), and the old acceptance test, reverting `Arr::swap` (`array.spp`) to `mem::ops::swap(self[mut i], self[mut j])`, is rejected by the compiler. `Vec::swap` (`vector.spp`) had the same live bug and was rewritten the same way `Arr::swap` was: an `i == j` guard plus one element borrow live at a time. Regression tests: `TestMemRegionOverlap::test_invalid_two_mutable_subscripts_of_one_value` and siblings in `tests/compiler/test_mem_region_overlap.cpp`; `regression::tst::codegen::test_compound_assignment_through_a_mut_borrow_parameter`. Related: [[spp-linear-types]], `std-known-gaps` (SPP-STL notes).
