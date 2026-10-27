---
name: closure-return-types
description: "Closures can declare a return type; without one their type is read off the body, which breaks variant returns"
---

Added 2026-09-01. `(x: S32) -> Opt[S32] { ret Some(val=x + 1) }` - an optional `-> Type` on a closure, which
**requires the braced body**, because the type is what a `ret` is checked against and there has to be somewhere to
write one. Parsed as two alternatives (`ParseClosureExpressionWithReturnType` / `..._without_...`).

The implementation is small because it reuses the subroutine path: `ClosureExpressionAst::Stage7` seeds
`meta->EnclosingFnRetType` with the declared type instead of leaving it empty, so `RetStatementAst` type-checks
the body against it and its `wrap_variant` coerces. Without a declared type the list stays empty and the first `ret`,
or the body expression, decides - which is the old behaviour, untouched.

## Why it was needed

A closure's return type used to be **only** inferable from its body, so `(x: S32) Some(val=x + 1)` is a
`Fun[..., Some[S32]]`. Passing it where `Fun[..., Opt[S32]]` is wanted type-checks (a member satisfies the variant)
but the representations differ, and nothing coerced: the IR had

```
define internal %"Some[S32]" @closure.fn(ptr, i32)     ; the closure returns the member
%closure.call = call %"Opt[S32]" %fn_ptr(ptr, i32 4)   ; the caller reads the variant
```

so the payload was read where the discriminant should be. That is what made `Opt::and_then`, `Opt::ior_else` and
`Res::and_then` fail - and note `ior_else` failed on the branch that *never calls the closure*, because the `case`
phi merged a real `Opt` against a `Some`-shaped value and corrupted both edges.

It also settles the inference complaint those tests carried: `and_then[U, ...]` had nothing to infer `U` from when a
closure body only produced `None()`, so those cases were commented out. The declared type supplies it, and they are
now written.

## Unrelated: `Opt::take_if` never emptied the slot

Its body matched, called the predicate, and returned `self` - a borrow - without writing a `None` back, despite the
doc saying it did. Rewritten to decide first and then delegate to `take`, which is the sibling that does the
`mem::ops::replace` properly.

**Update 2026-09-06:** the *lowering* half of this is fixed - a closure whose body builds one alternative of a variant
is now called through its own signature and its result widened, so `and_then((t: T) Some(val=..))` answers correctly
instead of falling through to the `unwrap_or` default (see [[spp-variant-widening-in-generic-args]]). The typing rule
itself is unchanged: a closure's type is still its body's, and writing `-> R` is still how you say otherwise.
