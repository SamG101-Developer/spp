---
name: e60-double-extension-order-dependent
description: "E60 double-extension was order-dependent (variadic pack only binds on the rhs); fixed by comparing both directions in b44966e3"
---

Fixed 2026-09-07 in `b44966e3`: `check_double` now compares the superclass first, then tries `RelaxedTypeEq` in both directions with a fresh inference map each way. The std duplicate it exposed (`Opt`/`Res` vs the blanket `Var` sup) went out as SPP-STL `71a95f1`. What follows is why it only showed up on CI.

`SupPrototypeExtensionAst::CheckDoubleExtension` called `RelaxedTypeEq(*ext->Name, *Name, ...)` — existing sup on the lhs, new sup on the rhs — and `MatchArgListArity` only lets a variadic pack swallow a list when the pack is on the **rhs**. So `sup [..Variants: Copy] Var[Variants]` vs `sup [T: Copy] Opt[T]` (= `Var[Some[T], None]`, see `TypeBinaryExpressionAst::Convert`) was detected in one attachment order and silently missed in the other.

Attachment order follows module order, which is `utils::files::GlobSpp` → `std::filesystem::recursive_directory_iterator`, never sorted (`module_tree.cpp`). ext4 htree order depends on the per-filesystem hash seed, so WSL and a GitHub runner enumerate `src/std` differently: locally `variant.spp` precedes `option.spp`/`result.spp` (no error), on CI the reverse (E60 on every unit test).

**Why:** this makes a whole class of semantic errors machine-dependent, and it is not a compiler bug you can reproduce by rebuilding.

**How to apply:** to reproduce a CI-only ordering failure, copy the project to the scratchpad and move the offending `sup` block into a module that globs later, rather than trying to reorder files (creation order does nothing on ext4). `GlobSpp` now sorts, so order is the same everywhere; the underlying order-dependence was fixed separately - see [[sup-attach-fixed-point]]. See [[spp-test-harness-stale-std]], [[sppc-sibling-repo]].
