---
name: drop-needs-forced-instantiation
description: "Destructors are reached only from C++ codegen, so every site that synthesises a drop must force-instantiate it during analysis"
updated: 2026-11-23
---

Nothing in S++ source ever names a destructor - `Drop::drop` is `!protected` and every discard goes through
`std::mem::ops::drop`, an intrinsic with no body - so **no call site ever resolves one**, and an overload no call
site resolves gets no `GenericSubstitution`. For a `drop` declared in a generic `sup` block (`Vec`, `Str`, `RawBuf`)
the only prototype that exists is then the template's, which is pure generic, never gets an `llvm::Function`, and
`EmitDrop` silently emits nothing. That was the Vec/Str buffer leak, fixed 2026-08-31 by
`drop_utils::EnsureDropInstantiated` + `monomorphization::RequireFnInstance`, hooked off the drop intrinsics in
`FunctionPrototypeAst::AnalysePendingGnSubstitutions`.

**Every site that synthesises a drop has to do this itself**, during analysis, with `EnsureDropInstantiated`:
the drop intrinsics (`_EnsureDropsForBuiltin`), every drop point of [[affine-ownership]] (`drop_utils::OwnedOnPath`,
`OwnedBeforeWrite`, `IsTemporary` do it for what they list), and a closure's by-value captures
(`ClosureExpressionCaptureGroupAst::Stage8_CheckMemory`). A miss today is not "nothing emitted" but worse:
`FindDropOverload` returns null, so `EmitDrop` destroys the value **field by field**, and a `Vec` frees its buffer
without dropping its elements. It only shows when nothing else in the program instantiated that type's `drop`, so a
probe that drops the same type elsewhere hides it (the unstarted `Vec[Noisy]::iter_mov` leak, 2026-11-15). Test a
new site with an element type nothing else in the program drops.

**The general rule this is an instance of:** any method the compiler calls from C++ rather than from analysed S++
needs its instantiation minted **inside the monomorphisation drain** (Stage 9.5). Minting it at Stage10/11 gives a
declaration with no body and an undefined symbol at link time. `Clone`, `Default` and anything else lowered the same
way will need the same treatment.

Two traps in doing it:
- `FindDropOverload` returns the **template** proto; the instantiation lives in `_GnSubstitutions` on it. Look it
  up with `FindGnSubstitution`, not by reading `GetLlvmFn()` off what the lookup handed you.
- The lookup and the minting must run through a scope manager on the **same** scope (the type's own `LinkedScope`),
  because `NormaliseGnArgs` rewrites the arguments against `CurrentScope` and the two must normalise alike or the
  lookup misses.

Diagnosing this: **per-module `out/llvm/**/*.ll` is the source of truth, not `lto.ll`** - a previous session read
`lto.ll` and concluded no drop symbols were instantiated at all, when in fact `drop[T=Vec[…]]` existed with an empty
body. See [[spp-ir-diff-verification]], `vec-start-removed`.
