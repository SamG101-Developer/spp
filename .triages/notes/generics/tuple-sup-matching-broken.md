---
name: tuple-sup-matching-broken
description: "Why tuple superimpositions were broken, the fixes, and the two known-red tuple tests that remain"
updated: 2026-08-25
---

Fixed 2026-08-25. `TestTupleSuperimpositions` went 1/11 → 10/11; `TestTupleNamedType` (new file,
`tests/compiler/test_msc_tuple_named_type.cpp`) is green; the sup file grew to 25 tests covering pack arity and
per-element constraints.

**The root cause was one invariant leaking.** A tuple is the only type whose generic arguments stay *positional* —
naming them is exactly what would fold the pack back into the tuple being named, so `NameGnArgs` returns early for a
tuple owner. Everything downstream that binds *by name* therefore mishandled a tuple. Four sites:

1. `FindOrMakeClsInstance` substituted into the prelude alias (`use std::tuple::Tup`) with positional args, leaving
   `Tup[Items=Items]` → `E26: Items unknown`. Fixed by replacing the arg group wholesale when `is_tuple`.
2. `RecursiveAliasSearch` passed a hardcoded `is_tuple_owner = false` to `NameGnArgs`, so `type MyPair = Tup[U64, U32]`
   resolved to a *nested* `Tup[Items=Tup[...]]`; and its `*generic_args += ...` merged positional args by name.
3. `RetrieveOwnerGenericArgs` fed a tuple's positional args to `MergeGenerics` (name-only) → internal compiler error.
4. The variadic arity check in **both** `TypeEq` and `RelaxedTypeEq` cast a *generic* parameter to
   `asts::FunctionParameterVariadicAst` — a cast that can never succeed, so it was dead code and `Tup[T, U, V]`
   matched a 2-tuple. Now one shared `MatchArgListArity` helper, which also lets a trailing argument naming a
   variadic parameter absorb the remainder. `PackConstraintsSatisfied` checks `..T: Copy` per element.

The pack is deliberately left **unbound** — binding it to a tuple makes `SubstituteGenerics` nest and recurse
forever, and the inference map holds borrowed `Ast*` so a synthesised tuple has no owner. A `sup` body that *names*
its pack is therefore not supported; that was the sprawl that got reverted once.

**Two tuple tests are red on purpose:**
- `variadic_empty_tuple` — `let a = ()` is rejected as an invalid primary expression long before sups; see
  `TestTupleLiteralAst.test_valid_empty_tuple_literal`.
- `param_name_shared_with_stdlib` — `sup [T, A] Tup[T, A]` captures `A`, so `std::vector::Vec[T=T, A=A]` analysed in
  that scope checks Vec's `A: Alloc` against the block's unconstrained `A` and raises E78. Renaming to `[P, Q]`
  avoids it. Not variadic- or arity-related: the same block over a user class is fine. Possibly a relative of
  [[spp-hasanalysed-conflation]].

Still latent, unreachable from these paths: `Raise<...>({}, ...)` with no scopes segfaults (`SemanticErrorBuilder::
Raise` cycles an empty formatter list, turning an internal error into a crash), and `EnforceGenericConstraintsOneArg`
derefs `concrete_sym` unguarded.

Regression check that worked well here: the pre-existing `tests/gtest-parallel-logs/failed/` directory is a usable
baseline — diff a run's failures against it rather than reading pass/fail counts. See [[build-workflow]],
[[add-tests-even-if-failing]].
