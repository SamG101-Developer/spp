---
name: spp-method-as-value-gap
description: "`Type::method` as a value works via nested-type naming (`main::A::$M`) + per-block mock coalescing + a mock per generic-owner instance; the traps hit on the way; `Type::method()` without self used to segfault"
updated: 2026-09-12
---

**Method as a value (done 2026-09-12, nested-type approach, the user's choice):** before this, `Type::method` as a value
never compiled (E26/E1): a method's `$` mock lives in its `sup` block and was only ever named bare. The pieces:
- The static access `InferType` (type case) returns `Owner::$M` (`TypePostfixExpressionAst`), built from AST copies.
  Its Stage 11 loads the mock constant instead of returning null.
- `TypePostfixExpressionAst::IsCompilerGeneratedType` checks the nested part, so the nested name is still treated as a mock.
- `TypeSymbol::FqName` qualifies a method mock through its owner, but only for non-generic owners. Generic ones, including
  defaulted ones like `Str[A]`, keep the bare name, because naming through `Str` walks the template, which lacks the
  instantiation's sup blocks.
- `SupPrototypeExtensionAst` registers nested `$` blocks in `NormalSupBlocks`, which used to take module-level blocks only.
  `AttachSpecificSuperScopesImpl` skips the cyclic, double and self checks for `$` blocks: bare `$New` resolves to another
  type's mock, which gave a false E59.
- `_DeduceMockClsType`: the kind condition was inverted (every method got FunRef). `self` now carries its convention,
  and `Self` is replaced by the owner.
- **Coalescing:** `ScopeManager::CoalesceMethodMock` runs after each attach and gives every same-named mock in the owner's
  other sup blocks' ext blocks to this one. `TypePostfixExpressionAst::Stage7` skips the nested-name ambiguity check for `$`
  names (it was E56). `GetFnValueName` returns the owner's class scope for a non-generic owner, so a call through a
  generic `F` gathers overloads from every sup block via `GetAllFnScopes`. `CoalesceMethodMock`'s guard must accept both
  `SupPrototypeFunctionsAst` and `SupPrototypeExtensionAst`; f448842a split it into two early returns, which disabled
  it silently and broke `spp test` (fixed 2026-09-27).
- Index and slice refuse a `$` lhs explicitly. Before this, the bare name failed to resolve, and that was what raised
  NonIndexable.

**Generic owners get a mock per instance (2026-10-01).** `TypeSymbol::NeedsDeepCopy` is true for `FnMock`, so a
cloned generic `sup` block owns its `$M`. `MakeSupInstance` re-links the copy to its cloned scope (`LinkedTypeSymbol` and
`LinkedScope`) and registers the cloned `sup $M ext FunXxx` blocks in `NormalSupBlocks` under the copy. Before this,
`Box[S32]::$Get` and `Box[Bool]::$Get` were one identity, with the function type still `FunRef[(&Box[T],), T]`.
`MatchFnValue` now reads the signature in the clone, where `T` is bound. **Do not resolve/instantiate the mock's
`FunXxx` at attach time** (`FindOrMakeTypeSymbol` there): every `FunXxx` instance has method mocks naming a bigger
`FunXxx`, so the sweep recursed into E109 on every program.

**Trap: shared cached FqName nodes.** Once the mock's function type named the owner (`FunRef[(&A,), Void]`), `A`'s cached
FqName node was analysed under AllowAbstractType. `cls B { a: A }` then hit `_HasAnalysed` and skipped the abstract check
(TestAbstract regression). Fix: `TypeIdentifierAst::Stage7` runs `check_abstract` on the early-return path too, so no
header change was needed. See [[spp-hasanalysed-conflation]].

**`Type::method()` without self (fixed):** `GetRequiredParams` skips `FunctionParameterSelfAst`. `ValidateArgsMatchParams`
now counts `self` as required, which gives E51/E47 instead of a segfault on `Args[0]`.

**FunMov risk (benign):** a named-function mock is stateless and `Copy`. A generic `FunMov` body is checked on the template,
so `f(f(x))` is still E5.

Related: [[spp-dollar-mock-types]], [[spp-generic-sup-cmp-and-void-params]].
