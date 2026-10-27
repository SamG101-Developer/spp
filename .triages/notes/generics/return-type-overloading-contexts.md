---
name: return-type-overloading-contexts
description: "Where a call overloaded only on its return type gets its expected type (meta->ReturnTypeOverloadResolverType), and the rule for when an expected type must NOT be offered (it depends on the argument itself)"
updated: 2026-09-28
---

`NarrowByReturnType` (overload_utils) narrows overloads by `meta->ReturnTypeOverloadResolverType`. It only narrows
when exactly one overload matches, so a bad expected type can't break a call; it can only fail to help.

**Contexts that set it:**
- `let x: T = g()`, assignment, `ret`, `gen`, `cmp`.
- **Call arguments (2026-09-28).** `PostfixExpressionOperatorFunctionCallAst::Stage7` sets
  `FnArgGroup->ExpectedTypes = overload_resolution::ExpectedArgTypes(...)` before analysing the arguments.
  `FunctionCallArgumentGroupAst::Stage7` applies each one to an argument that is itself a call.
- **Object-initializer attributes.** The Stage-6 pre-pass receives the written type, generics included, in
  `meta->ObjectInitType`. It names the written generic args against the class's parameters and substitutes them into
  the attribute's type (`MyType[T=Str](a=g())`).

**Only offer an expected type when it does not depend on the argument (the maintainer's "2-way" issue):**
- **Generic parameter** (`h[T](x: T)`): stays ambiguous.
- **Overloads of the callee disagree** at that position: stays ambiguous. Checked with `TypeEq` across every
  candidate.
- **Parameter names `Self`, is variadic, or isn't `IsTypeFullyConcrete`:** skipped.
- **Unbound or partly bound generic attribute** (`MyType(a=g())`): stays ambiguous.
- **Tuple expansion (`..t`) in the call:** skipped entirely, because positions don't line up before expansion.

**Traps:**
- An attribute typed `Opt[T]` resolves to `Var[..Some[T]..]`, whose symbol is not `IsTypeGeneric`. Decide by
  concreteness after substitution, not by `IsTypeGeneric`.
- Analyse only the substituted copy, never `sym->FqName()` itself (see [[typeeq-accepts-forwarding]] and the shared
  FqName trap in [[generic-inference-engine-shape]]).
- `Opt[Str]` accepts both a `g() -> Opt[Str]` and a `g() -> Str` (variant widening), so that call rightly stays
  ambiguous.
- `ExpectedArgTypes` swallows `SemanticError`: candidate lookup errors belong to real resolution, after the arguments
  are analysed.
