---
name: overload-arg-matching-order
description: "The order arguments are matched against parameters in ValidateArgsMatchParams, and which orderings break"
---

`ValidateArgsMatchParams` (overload_resolution.cpp) matches each argument in a fixed order, and the order is load-bearing:

1. A **`self` parameter** is not type checked at all — the receiver is what chose the overload. It only takes the convention the prototype declares.
2. **Exact match** — `ConventionEq(param, arg)` then `TypeEq(param, arg)`.
3. **Relaxed match** — binds generics the call is free to choose. Called **argument-first** (`RelaxedTypeEq(a, p, caller, fn)`), which is the order it infers the *parameter's* generics from the argument in; `RelaxedTypeEq`'s internal `ConventionEq(rhs, lhs)` is inverted to compensate for exactly this caller.
4. **Forwarding**, as a *last resort* before raising.
5. Separately: when 2 succeeded, forwarding is still tried, because a type can match exactly and still need the forwarded value.

**Why:** trying forwarding *before* the relaxed match (step 4 moved ahead of 3) rewrites arguments that the relaxed match would have bound correctly and **segfaults about a third of the suite** — 342/376, with `vector`, `string`, `iterator` and `fs::path` all failing. Removing step 5 on the assumption it is dead breaks the same set. Both were tried and reverted 2026-09-02.

**`Self` resolution:** a `&Self` parameter resolves to the *owning* type when the call-site receiver only reaches it by forwarding (`TypeFwdEq(receiver, owner)`), and to the receiver otherwise — `Self` on an abstract sup block is meant to be the concrete receiver. Note `ReceiverTypeAtCallSite` returns null when the left-hand side is a plain identifier rather than a postfix expression, so the owner is the fallback there too.

**`RelaxedTypeEq`'s asymmetry is real, and measured.** Instrumenting a full suite run: a generic on the **right** fires ~155k times (a pattern accepting the left — `sup [T] T { }`, or a parameter taking an argument) and is settled *before* the convention check; a generic on the **left** fires ~2.7k times (a sup block's own name or a function's owner against a concrete type) and only *after* it. 869 of the right-hand matches would fail the convention check if it were hoisted above them, so the ordering is load-bearing — "make it left-relaxed only" is not available without rewriting the sup-attachment callers. `strict_generic_args` likewise stays: only `scope_manager`'s sup-attachment calls pass it, and they need an unbound parameter to stop matching a type written opposite it. The one thing that *was* removable is the inverted `ConventionEq(rhs, lhs)`, which existed to suit the param/arg checker's argument-first operands — zero calls in the whole suite disagreed with the natural order, so it is now written `ConventionEq(lhs, rhs)`.

**How to apply:** change this loop only with the full std suite as the check, and change one thing at a time — the failure modes are segfaults in unrelated modules, not type errors. Related: `std-known-gaps` (SPP-STL notes).
