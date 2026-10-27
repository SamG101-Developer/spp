---
name: abstract-types-by-value
description: "Abstract-type rule since 2026-09-28: naming an abstract type as a generic argument is fine; a type is rejected only if it holds one by value (FindHeldByValue). Why stage-5 instances needed this"
updated: 2026-09-28
---

`TypeIdentifierAst::Stage7`'s `check_abstract` works like this:
- **Generic arguments:** a type's generic arguments are analysed under `AllowAbstractType`. `B[A]` with an abstract `A`
  is fine when `B` holds no `T` by value.
- **Held by value:** a type is instead rejected when it, or anything it holds by value, is abstract, via
  `type_members::FindHeldByValue(ref, scope, predicate)`. So `C[A]` with an attribute `t: T` is E45 wherever it is
  written, e.g. as a parameter type.
- **Shared walk:** `FindHeldByValue` is the recursive-type walk (`FindRecursiveAttrType`) generalised to a predicate. Use it
  for any other "does this type contain X by value" check.

**Why the by-value check is on the type and not left to the instance's attributes:** an instance first created while
signatures are analysed (stages 5/6) has its attributes analysed below `kPreAnalyseSemantics`, where `check_abstract`
returns early, and it is never re-analysed. The old E45 on `C[A]` only ever came from the (now-allowed) argument check.
`FindOrMakeClsInstance` also resets `AllowAbstractType = false` for attributes, so an instance minted while analysing
another type's arguments (`B[C[A]]`) is not let off.

Related: [[spp-hasanalysed-conflation]] (the shared-FqName abstract-check trap).

**Open instances are not judged (same day, #11/#12).** `check_abstract` returns early unless the resolved type is
`IsTypeFullyConcrete`. A template's open instance (`Vec[Box[T]]` inside `Box`'s own generic sup) doesn't have the
superimpositions that depend on `T` yet, so it looked abstract (unimplemented `iter_mov`). Don't trust the symbol's
`IsConcrete` flag for this: an instance minted through an alias had it set despite containing `T`.

**Inference sources point at their argument.** Argument types fed to generic inference (object-initializer and
call arguments) are stamped with `TypeAst::WithSourceSpanAt(*arg->Val)`. Otherwise a literal's type is its symbol's
generated `FqName`, and E71 conflicts pointed at "<generated code>", which `SHOULD_FAIL` tests reject.
