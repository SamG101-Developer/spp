---
name: void-is-no-value
description: A Void value is no value: Void params stripped (fns and closures), Fun* types drop Void from Args, a Void arg with no param is unbound but still evaluated
updated: 2026-11-23
---

A value of type `Void` is no value. The rule has a parameter side, a type side and an argument side, and they have
to agree, or a callable stops matching its own type:

- **Parameters.** One of type `Void` is no parameter: `FunctionParameterGroupAst::RemoveVoidParams` strips it from a
  function prototype (Stage 6, and each generic instance in `monomorphization`), and from a closure
  (`ClosureExpressionParameterAndCaptureGroupAst::Stage7`, since 2026-11-23). The symbol stays, as the body may name it.
  So `fun k(v: Void)` and `|v: Void|` take no argument, and generate no LLVM parameter.
- **Function types.** `FunMov`/`FunMut`/`FunRef` drop `Void` from their `Args` tuple when keyed
  (`WithoutVoidFnArgs`, called by `InstanceIdOfArgs` in `type_key.cpp`), so `FunMov[(Void,), U]` is
  `FunMov[(), U]`. This is what lets `Res[Void, E]::map(|| ...)` meet `F: FunMov[(T,), U]` at `T = Void`.
- **Arguments.** `FunctionCallArgumentGroupAst::IsVoidArg` is the one predicate. A `Void` argument landing on a
  parameter is bound as usual (so `take(nothing())` infers `T = Void`), and that slot is then dropped
  (`slot_is_void` in `ValidateArgsMatchParams`). One left with no parameter is no argument: `BindSlots` leaves it
  unbound rather than raising E50, and `ElaborateCall` keeps it after the slots, because it is still evaluated.
  Codegen generates every argument and passes only the values (`_ArgValues`, used by direct and closure calls).
  So `k(nothing())` with `fun k()` calls `nothing()`, then `k()`. A candidate that takes the argument beats one that
  leaves it unbound (`MatchRank::Unbound`, the worst rank): `drop(a)` with `a: Void` is `drop[T](val: T)`, not std's
  zero-parameter `drop()`, which would otherwise tie with it (E52).

A positional `Void` argument still binds positionally: `f(nothing(), 1)` with `fun f(x: U32)` binds `nothing()` to
`x`, leaving `1` with no parameter (E50, inside E51); it isn't skipped.

**Why:** with only the parameter side, `|v: Void|` generated an LLVM `void` argument (ICE), `map` on `Res[Void, E]`
failed its constraint or segfaulted passing a null `Void` argument, and `k(nothing())` was E50.

**How to apply:** a new call path or callable kind goes through `IsVoidArg`/`RemoveVoidParams` and `_ArgValues`, not its own
`IsTypeVoid` check. A stripped parameter in a generic instance has the moved-state trap in
[[spp-generic-sup-cmp-and-void-params]].
