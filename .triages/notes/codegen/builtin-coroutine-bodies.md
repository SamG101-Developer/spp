---
name: builtin-coroutine-bodies
description: "A builtin coroutine's hand-built gen is never analysed: give it SetOwnedAtSuspend, and index a by-move self's slot directly"
updated: 2026-11-15
---

A builtin coroutine (`!intrinsic` on a `cor`, e.g. `Arr::iter_mov` in `fn_impls::SimpleCoroIter`) has its body built
at codegen out of mock `GenExpressionAst`s wrapping a `CustomExpr`. Two things the analysed path does for a written
`gen` do not happen for these:

- **Nothing records what the frame owns.** `_OwnedAtSuspend` is filled at Stage8, which a mock `gen` never sees, so
  its destroy block was empty and a loop left part-way leaked every element not yet yielded. Set it with
  `GenExpressionAst::SetOwnedAtSuspend` before each `Stage11_CodeGen`: the owner (`SELF_VAR`) with the parts already
  moved out as `Moved` paths (an array's parts are named `"0"`, `"1"`, ... by `GetAllParts`). `EmitSuspendDestroy` then
  drops the rest through `EmitDrop`'s partial-move path. The initial suspend needs nothing: the prototype is analysed,
  so `_OwnedAtStart` (and the destructor instantiation) come from `OwnedAtSuspend` as usual.
- **`self`'s slot holds what the convention says.** By move, the alloca holds the value itself (`[n x T]` for an
  `Arr`), so GEP straight off `self_sym->LlvmInfo->Alloca`. Only a borrowed `self` holds a pointer to load first.
  `SimpleCoroIter` loaded one anyway, reading element 0's first 8 bytes as the array's address (unsafe-audit #8).

**Why:** the unit tests only build a builtin coroutine, and nothing ran `Arr` move iteration with elements that own
memory, so both stayed hidden until the audit's `Str` probes segfaulted, read past a buffer, and leaked.

**How to apply:** for any new builtin `cor`, test it with an element type that owns something (`Noisy` in
`tests/codegen/check-drops.sh`), run to completion, left part-way, and never resumed. See [[nonnull-lowers-to-ptr]] for
the by-value vs by-borrow load rule on `NonNull`, and [[drop-needs-forced-instantiation]].
