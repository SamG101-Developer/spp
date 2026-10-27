---
name: borrow-is-param-convention
description: "s++ semantics: a borrow is bound on the parameter's convention, not the generic type; f[T](x: T) cannot take &Str (that needs x: &T), so inference rejecting f(&s) is correct"
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
- Whether an explicit `f[&Str](&s)` should be allowed at all is unconfirmed; it compiled on 2026-09-28. Ask the maintainer
  before changing it.

Related: [[typeeq-accepts-forwarding]].
