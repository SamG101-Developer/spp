---
name: spp-generic-sup-cmp-and-void-params
description: "`cmp n: T = m` in a generic sup has no global (folded at the use site); a Void-stripped parameter keeps the template's moved state"
updated: 2026-10-26
---

Fixed 2026-09-12, found once the gtests ran codegen (see [[spp-test-suite-stage-scope]]).

**Generic-typed `cmp` in a `sup` block** (`sup [T, cmp m: T] MyType[T, m] { cmp n: T = m }`): only the template's
`CmpStatementAst` reaches Stage 10, and `T` has no LLVM layout there, so `Stage10_PreCodeGen` now returns early when
`llvm_type` is null. There is no global for `n`, so `MyType[S32, 123]::n` (static access, type case) folds via
`EvaluateCompTime` and emits the constant in place when `LlvmInfo->Alloca` is null. The fold needs
`IdentifierAst::EvaluateCompTime` to fall back to `VariableSymbol::BoundCompValue()`: a comp generic's value is in
`MemInfo->AstCompTime`, not `CompTimeValue` (that raised E3).

**`Void`-stripped parameters** (`take[T](t: T) -> T { ret t }` at `T=Void`): `StripVoidParams` removes `t`, so its
Stage 8 never re-initialises the symbol, which still holds the template's "moved by `ret t`" → E5.
`AnalysePendingGnSubstitutions` now initialises every template parameter's symbol first.

**Variadic forwarding:** `A[T]` inside `g[..T]` forwards the pack in `NameGnArgsImpl` instead of wrapping it as
`Tup[T]`. The bound symbol keeps `IsVariadic` (RegisterGnSymbols), and `type_compare`'s pack rule is limited to
unbound symbols.

**Coroutine closures** (`cor (...) { }`) raise `SppFeatureNotYetSupportedError(CoroutineClosure)`: closure codegen never
sets up generator state. The two `cor` closure tests expect E100 and carry Todos to switch back.
