---
name: drop-needs-forced-instantiation
description: "Destructors are reached only from C++ codegen, so they must be force-instantiated during monomorphisation"
updated: 2026-08-31
---

Nothing in S++ source ever names a destructor - `Drop::drop` is `!protected` and every discard goes through
`std::mem::ops::drop`, an intrinsic with no body - so **no call site ever resolves one**, and an overload no call
site resolves gets no `GenericSubstitution`. For a `drop` declared in a generic `sup` block (`Vec`, `Str`, `RawBuf`)
the only prototype that exists is then the template's, which is pure generic, never gets an `llvm::Function`, and
`EmitDrop` silently emits nothing. That was the Vec/Str buffer leak, fixed 2026-08-31 by
`drop_utils::EnsureDropInstantiated` + `monomorphization::RequireFnInstance`, hooked off the drop intrinsics in
`FunctionPrototypeAst::AnalysePendingGnSubstitutions`.

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
