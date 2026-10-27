---
name: generic-alias-constraints
description: "Aliases in constraint/callable positions: aliases::TargetOf for the written-head trap, call-site constraint checks resolve from the argument's scope, and constraints are re-analysed in Stage 7"
updated: 2026-09-28
---

Three gaps closed on 2026-09-28 (#10 in .triages/open-bugs.md):

- **Written-head trap.** `TypeRef::OfHead` of `Closure[(), Void]` is the *alias* symbol, so a check on the head
  (`IsTypeFunction`) says no, even though `TypeRef::Of` of the whole type already resolves to the `FunMov` instance.
  `type_resolution::aliases::TargetOf(type, scope)` returns what an alias-headed type stands for (a copy, never the shared
  `FqName`). It is used by `FindFnSup` (template body calls) and `ReattachIndirectFnTypes` (instances).
  Any other "is the head X" check on a user-written type needs it too.
- **Call-site constraint checks resolve from the argument's scope.** `EnforceGenericConstraintsAllArgs` analyses the
  substituted constraint in `arg_sym->LinkedScope`. For a closure argument that is the global mock scope, where a
  module-private alias is invisible. It now sets `IgnoreAccessModifierViolations`: visibility was already checked
  where the constraint is written.
- **Constraints never had their own generic args checked.** They are analysed in stage 4, and the check needs stage 6+.
  `GenericParameterAst::Stage7` now `ResetCache()`s and re-analyses each constraint (with `AllowAbstractType`), so
  `F: Kk[S32]` with `Kk[Ts: TupLike]` is E78. `ResetCache` only clears `_HasAnalysed`; stamps survive.

**Language fact:** `(S32)` in a type is a parenthesised `S32`, not a one-element tuple. That is `(S32,)`.

Related: [[use-imports-are-alias-symbols]], [[return-type-overloading-contexts]].
