---
name: open-instances-made-by-analysis
description: An open instantiation (naming a template's own parameter) exists only once its type is analysed; a type spelled from an identity must be analysed before it is resolved
updated: 2026-11-23
---

`FindOrMakeTypeSymbol` (symbols.cpp) makes a missing instantiation on read only when its identity is **closed**
(`IsClosedTypeId`). An **open** one, naming a parameter of the template being analysed (`Res[K[A], E]` inside
`fun f[A]`), is made by *analysing* the type (its `Stage7_AnalyseSemantics`), which a written type always is. And
`TypeRef::Of(TypeId, Scope)` never makes anything.

So a type the compiler builds itself, rather than one the user wrote, has no symbol in a template until something
analyses it. A call's return type spelled from an identity (`InferType`'s `TypeAstOf(ret_id)` for a `Res[Self, E]`
called through `K[A]`) was resolved without that. `InferTypeRef` gave a `TypeRef` with no symbol, and the next member
access (`.expect`) dereferenced null (shield `spp test` segfault, fixed 2026-11-23). `InferType`'s other paths
(the fold tuple, the void type) already analysed what they built.

**Why:** an open instantiation lives in the template's scope only once analysis puts it there. Reading by identity
assumes it's been made.

**How to apply:**
- A type built from an identity (`TypeAstOf`, a substitution's result) that the analysis then resolves: analyse it
  where it's built.
- When a `TypeRef` comes back with no symbol inside a template, look for a built type that was never analysed, before
  suspecting the identity.
- `PostfixExpressionOperatorRuntimeMemberAccessAst` now ICEs on a symbol-less left-hand side instead of crashing.
