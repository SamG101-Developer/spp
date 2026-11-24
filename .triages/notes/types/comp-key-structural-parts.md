---
name: comp-key-structural-parts
description: A comp value without a structural CompKey part is Opaque, keyed by its spelling: unifies only with the same spelling, never rewritten; arrays got Part::Array 2026-11-22, objects still opaque
updated: 2026-11-22
---

`RawCompKey` (`sources/spp/analyse/scopes/scope.cpp`) gives a comp value a structural key only for the shapes it knows:
a literal (`Part::Value`), a parameter (`Param`), a pack's list (`Pack`), an operation (`Op`), a constant named through
a type (`Member`), a tuple (`Tuple`) and an array (`Array`). A repeated array whose count is a value is keyed as its
copies, so `[1_u64; 2_uz]` and `[1_u64, 1_u64]` are one value. Anything else falls through to `RecordedOpaque`, so it
is `Part::Opaque` keyed by its spelling. That is objects (`Some(val=n)`), calls in a template (`f(n)`, folded only where
concrete) and a repeated array whose count is a parameter.

An `Opaque` key:
- unifies only with an identical spelling, so a `sup [cmp n] A[Some(val=n)]` never matches (E26 on its methods);
- is never rewritten under a substitution (`RewriteCompId` returns it as is), so `cls B[cmp n] { a: A[Some(val=n)] }`
  keeps the `n` (E1);
- doesn't normalise literals, so `Some(val=1_u64)` and `Some(val=0x1_u64)` are two types.

Before 2026-11-22 arrays were `Opaque` too, which is how all three were found. Objects are written up in
`../../docs_open/comp-object-keys.md`, with the design questions still open.

**Why:** a spelling is not a value, and the type system compares comp arguments by key.

**How to apply:** a new comp-value shape that must unify, substitute or compare by value needs a part in all of
`CompKey::Part` plus an `Of*` constructor, `RawCompKey`, `UnifyCompIds`, `RewriteCompId`, `CompAstOf` and
`IsValueCompId`. Copy `Part::Array`, which shares the tuple's case labels. A shape `type_predicates::AnyCompPart` doesn't walk
is also invisible to `DoesTypeNameAGnParam` (E65 on the `sup`) and to stamping.
