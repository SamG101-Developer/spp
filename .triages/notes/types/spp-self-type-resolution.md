---
name: spp-self-type-resolution
description: "Self is registered in stage 3 and substituted per written-type position; why it cannot be centralised in FqName"
updated: 2026-09-14
---

`Self` is a real `TypeSymbol` registered into the class or superimposition scope, linked to the scope of the type the
block belongs to. **It is registered in `Stage3_GenTopLvlAliases`** (2026-09-10; it used to be `Stage5_LoadSupScopes`),
via `ScopeManager::AddSelfTypeSymbol`, in `ClassPrototypeAst`, `SupPrototypeFunctionsAst` and
`SupPrototypeExtensionAst`. Stage 3 registers a provisional one off `Name->WithoutGns()`, because the name is not
qualified yet; stage 5 re-registers it against the resolved name, and `SymbolTable::Add` overwrites. This is what makes
`type Mine = Self` resolve at all - aliases are resolved in stage 3, so a stage-5 symbol was simply not there yet.

**The substitution is per written-type position and cannot be centralised.** There are ~14 calls to
`ResolveAndSubstituteSelfType` on `Shared<TypeAst>` fields, and that repetition is not an oversight:

- `TypeSymbol::FqName()` deliberately answers `Self` as-is. Making it answer `LinkedScope->LinkedTypeSymbol->FqName()` instead
  **breaks the whole std** - for a generic class, that name is parameterised, so `Self` inside `cls Eq[Rhs=Self]`
  becomes `Eq[Rhs]` and `Rhs` escapes into every context the name travels to ("Rhs is not defined in the current
  scope"). Tried and reverted 2026-09-10.
- The reason is that what `Self` means depends on the scope the type was **written** in, while `FqName()` is a property
  of a symbol read from anywhere. A generic parameter default is the sharpest case: `sup Bool ext Eq` needs `Rhs=Self`
  to become `Bool`, so the default's `Self` is bound where the generics are bound, not where it is written.
- `Stage4_QualifyTypes` is not a hook either: it descends and syncs visibility, and a `TypeAst` cannot replace itself -
  the owner holds the field, so a stage-4 pass would be the same 14 sites in a different stage.

`SubstituteSelfType` (no analysis) is split out of `ResolveAndSubstituteSelfType` for callers that run before a type
can be analysed - the alias target in stage 3 is the one that needs it. Substituting in `TypeStatementAst::Stage3` is
what fixes aliases; the type statement's own stage 7 is **too late**, because the bodies naming the alias are analysed
in the same stage and reach it first.

**Substitute where a written type is first analysed, not where you expect.** Two positions were resolving "Self"
too late, and both were found by writing `Vec[Self]` rather than a bare `Self`:

- A **class attribute**: `ClassAttributeAst::Stage5_LoadSupScopes` analyses the type, not stage 7. Leaving "Self" in
  meant `Vec[Self]` was monomorphised with "Self" itself as the argument, and every scope that instantiation reached
  re-resolved it against its own type - the attribute's type grew into `RawBuf[RawBuf[..]]` until the stack ran out
  (`_Depth` in the meta history is the tell). Substituting in stage 5 fixes it. A **bare** "Self" is still left
  standing there: an attribute of the type being declared has no size, and stage 7 reports that.
- A **function parameter**: the signature deliberately keeps "Self", so an abstract method's means the implementing
  type; it is decided per call in `ValidateArgsMatchParams`. That code handled a bare "Self" only, so `Vec[Self]` was
  matched as the literal `Vec[T=Self]` and every argument mismatched. It now picks what "Self" stands for (owner vs
  call-site receiver, the forwarding logic behind the `Str == Str` bug) and substitutes every part naming it, via
  `SubstituteSelfTypeWith`.

**Since 2026-09-14 the per-site sequence is one helper**, `type_resolution::FindWrittenTypeSymbol(written, sm, meta, SelfPolicy)`: substitute "Self" (no analysis) → Stage7 on the result, with `AllowAbstractType` only if something was substituted → symbol `FqName()` → the written convention → `WithSourceSpanOf(written)`. It analyses a clone, so the written node stays as written (that is what makes "Vec[Self] (aka ...)" print). Used by both lets, cmp, case pattern, let destructure, comp generic param, closure return, postfix type lhs, class attribute (`kKeep` for a bare "Self"), and function parameter/return (`kKeep`, decided per call). Deliberately not used: the object initializer (substitutes only - the helper would allow `Self()` on an abstract class), generic arguments (the InnerScope rule), sup headers (`FqName(true)`), type statement stage 3.

A call's argument coercion (`PostfixExpressionOperatorFunctionCallAst::Stage11_CodeGen`) substitutes "Self" with the owner from the overload scope, matching `_IsPureGn`'s signature; without it "x: Self or S32" widened into `Var[Self, S32]` and LLVM rejected the call.

Positions still broken (red in `test_msc_self_type_positions_generic.cpp`, each with a Todo): `type X = Self`/`Vec[Self]` in a generic sup (E72, double substitution). The generic-class destructure, comp-param and closure failures there are not "Self" bugs - the same code with `Box[T]` fails identically.

Related: [[spp-dollar-mock-types]], [[spp-visibility-stage-ordering]].

**Decided once per call (2026-09-28).** `DetermineOverload` decides what `Self` stands for per candidate:
- the resolved receiver of `Type::method(..)`, unless the candidate was reached by forwarding (`FnOverload::FwdType`);
- otherwise, or with no receiver, the instance scope's enclosing `Self`.

It is carried as `PassedOverload::SelfType`, then `_OInfo::SelfType`. `ValidateArgsMatchParams`, `NarrowByReturnType` and
the call's `InferType` read it instead of re-deriving it. Two traps found on the way, each giving E83 in `socket.spp`
(a `!package` field read as private):
- **Unmarked value:** the decided type is cloned with every part's `ClearSourceWritten()`. A symbol's name is written
  where the symbol is declared, and would answer access checks as if written at the call.
- **Fresh return type:** a call through a receiver (`a.m()`, `A::m()`) must return a fresh, unstamped copy of its
  return type (`SubstituteGenerics`), *even when it names no `Self`*. Returning the stamped, shared `FqName` node broke
  access checks downstream, although the spelling was identical.

`PinSelfToReceiver` (instance identity) is unchanged.

**Signatures bind `Self` themselves (2026-09-29).** `SameSignature(fn_a = override/impl, fn_b = base)` compares in `fn_a`'s terms:
- `Self` becomes the implementing class.
- `fn_b`'s block parameters are bound by matching its sup pattern against the super class the implementer extends (`ext Eq` is `Eq[Rhs=Self]`).
- `fn_b`'s own method generics are renamed to `fn_a`'s by position.

That removed the only load-bearing use of equality's one-sided "`Self` means the other side's class", so written-type `TypeEq` could become `TypeId` equality. Override return types are covariant.

**One decision per level (2026-09-29):**
- **Per call:** `CallSelf` feeds the pin and `PassedOverload::SelfType`.
- **Per instance:** `instance_self` and `pinned_self` are read once in `PotentiallyGenerate…` and passed to `RetypeInheritedSymbols` and `ComputeIsConcrete`.
- **Per default:** `GenericSolver::_SelfForDefault`. A value default gets the resolved owner; a type default keeps `Self` as spelled.

**Kept on purpose:** the `Self` block in `SameType`/`AssignableCore` (`ReadSelfPair`). Removing it (with `TypeRef::Of` answering `Self` as its class) broke codegen: `Opt[Self]` and `Opt[Char]` are two instances under the spelling key, and the "no coercion" check needs them equal. `SameSignature` also binds a base parameter bound to `Self` (like `Ret` of `BitAnd`) to the implementer.
