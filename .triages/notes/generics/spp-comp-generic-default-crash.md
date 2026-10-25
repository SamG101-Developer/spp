---
name: spp-comp-generic-default-crash
description: "Comp generic defaults naming Self - fixed by expression-level substitution; the Stage11 fold is still needed"
---

`Atom[S32]::new(7).load()` used to segfault Stage7, because `load[cmp order: U8 = Self::mo_seq_cst]` reached `InferGnArgs` with `Self` unsubstituted. **Fixed properly (2026-09-02).**

1. **Resolve.** `ExpressionAst::ReadExpr` now exists — named apart from the type-level `SubstituteGenerics` because `TypeAst` is both a type and an expression and would otherwise inherit two. `InferGnArgs` substitutes comp defaults against the full binding set plus the `Self` its owner resolves to, rather than pattern-matching one shape. The old `ResolveSelfInCompDefault` band-aid and its ownership vector are gone, as is `asts/utils/generic_substitution`.

2. **Fold — still needed, do not remove.** `StaticMemberAccessAst::Stage11_CodeGen` folds via `Stage9_CompTimeResolve` when `ctx->InConstantContext`. A generic parameter group is **never walked at Stage9**, only Stage11, so a bound value that is still an expression would arrive at code generation as an *access* with nothing to load. Both setters of that flag need a constant rather than a load: `CmpStatementAst` emitting a bound comp value, and `AtomicOrderingOf`, which casts the result to `llvm::ConstantInt`.

**`is_lock_free` was a separate bug, also fixed.** It is the one *static* intrinsic in the atomics — no `self` — so `Self` is not registered in its own scope. It asked `FindTypeSymbol(SELF_TYPE, /*exclusive=*/true)`, and `exclusive` stops the walk before reaching the sup block that declares `Self`; the null went straight into `GetLlvmType`. Every working caller omits the flag. **An exclusive symbol lookup inside an intrinsic is nearly always wrong.**

**How to apply:** the whole `std/threading/atomic` group now runs on declared defaults with no ordering written out. If a comp default regresses, check the Stage11 fold before the substitution — the two fix different stages and neither subsumes the other. Related: [[getallparams-includes-self]], `expression-generic-substitution`.
