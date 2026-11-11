---
name: borrow-is-param-convention
description: "s++ semantics: a borrow is bound on the parameter's convention, not the generic type; f[T](x: T) cannot take &Str (that needs x: &T), so inference rejecting f(&s) is correct"
updated: 2026-11-03
---

In s++ a borrow is part of the **parameter's convention**, never of the generic type argument. `fun f[T](x: T)`
cannot accept `&Str`: taking a borrow needs `x: &T`.
- **Inference is right to reject `f(&s)`:** it binds `T=Str` and the call fails E51. That is correct behaviour, not a
  bug.
- **Stated by the maintainer, 2026-09-28**, when I flagged the E51 as a possible inference gap: "the borrow is bound on the param
  not the type".

**How to apply:**
- Don't treat convention-stripping inference as a bug.
- Don't propose inferring `T=&X` from a borrow argument.
- An explicit `f[&Str](&s)` is E2 since 2026-11-03 (P0): a borrow is no generic argument, outside a generator's or a
  function type's ([[borrow-not-a-generic-argument]]).

Related: [[typeeq-accepts-forwarding]].
