---
name: typeeq-accepts-forwarding
description: "Since 2026-09-28 TypeEq IS identity and Assignable(target, value) carries forwarding/variants/&mut→&/mocks; before that TypeEq accepted &Single[S32] for &S32 and needed calling both ways"
---

On 2026-09-28 (audit F3), `type_compare` was split into two predicates:
- **`TypeEq`** is symmetric identity (`SameType`). Conventions must match exactly, arguments compare invariantly, and `!` equals only `!`.
- **`Assignable(target, value)`** keeps every looseness the old `TypeEq` had:
  - `!` fits anywhere;
  - `&mut` is accepted for `&`;
  - variant widening at the top level and inside arguments;
  - `$` mocks;
  - forwarding (`Assignable(&S32, &Single[S32])` is true).

The `check_variant` flag on `TypeEq` is gone. Calling `TypeEq` both ways is no longer needed for a "same type" test.

**Why this mattered before:** on 2026-09-27, a "yield as-is, forward only as a last resort" check in `gen_expression_ast.cpp` used a single loose `TypeEq`. Every `gen self` in `Single::get_ref`/`get_mut` skipped the FwdRef call, and 4 std `single` tests crashed at runtime. That site is now one strict `TypeEq`.

**How to apply:**
- Choose deliberately. If the question is "is this the same type", use `TypeEq`. If it is "can this value go there", use `Assignable`, with the wanted type first.
- A comparison that only works loose is usually hiding a bug. `RawBuf`'s unsound `Indexed[&T or None]` override was one.
- A prelude name like `Str` is still an alias symbol, so compare through these predicates, not by symbol pointer (see [[use-imports-are-alias-symbols]]).
