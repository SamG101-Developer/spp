---
name: spp-same-scope-shadowing
description: "How a name means the right binding after Stage 7: lets declare at their position in every walk (M2, 2026-10-10)"
updated: 2026-11-24
---

A scope's symbol table holds one symbol per name (`Add` replaces), so after Stage 7 a name belongs to the *last*
binding declared in the scope. Until 2026-10-10 that was patched per-ast (`_PrevSymbol`/`_ExposePreviousSymbol`/
`_UsesSavedSymbol`, a `PosStart` check in `CheckLiveUpTo*`, an init take-down). M2 of
`.triages/docs_closed/memory-analysis-refactor.md` replaced all of it with one model:

- Stage 7 records each `let`'s binding on its scope (`Scope::RecordLetBinding`). `MoveToNextScope` hides them
  (`HideLetBindings`); each `let` redeclares its own once its value is done (`LocalVariableSingleIdentifierAst::_Declare`),
  in Stages 8, 9 and 11 and in every temporary `ScopeManager`.
- Leaving or skipping a scope reveals them, last one winning (`MoveOutOfCurrentScope`, `SkipSubtree`):
  outside a walk a scope reads as Stage 7 left it. Needed because a value's type is inferred before it is walked.
  Skip loops must use `SkipSubtree`, never `while (..) MoveToNextScope()` (that hides without revealing).
- Anything that adds a symbol directly (not via a `let`) that a later `let` of the same name replaced must redeclare it
  in every walk at its position: case-pattern flow symbols (`_DeclareFlowSymbol`), destructure cond lets (`Narrow`).
  Bindings declared up front by an owner (params, closure captures) are `UnrecordLetBinding`-ed.
- A hidden symbol a desugaring makes is recorded too: the destructure temporary `$_dst_<uid>`
  (`destructure_utils::BindDestructureTemporary`) is a let binding, redeclared by `DeclareDestructureTemp` after its
  value is checked (Stage 8), and before use in comp-time and Stage 11. Unrecorded (before 2026-11-21) it sat in the
  scope from Stage 7, initialized, so every `ret` or `?` before `let (a, b) = mk()` raised E94 "Return reached with
  `$_dst_$N` still holding it" (under affine ownership it would drop an uninitialised slot instead).
- An `is` binding (`VariableSymbol::ExpiresWithStatement`) lasts to the end of its statement only. When it takes a
  name already held, in this scope or further out (a parameter, the subject a flow-typed name narrows),
  `IsExpressionAst::_DeclareExposed` logs the holder in `meta->DisplacedByIsBindings` in every walk, and
  `InnerScopeExpressionAst` puts it back after each member (`RestoreDisplacedByIs`: a holder of this scope is added
  back; for one further out the binding is removed, so the name reads outward and the scope adopts no owner). An `is`
  binding owns nothing (a view, see [[affine-ownership]]), so a statement's end drops none. `RevealLetBindings` and
  `_Binding` pass `is` bindings over for a name that has another binding. Before 2026-11-22 the `let` stayed displaced: it could not be named
  afterwards (E26), and its value was never checked at scope end.
- A displaced binding is still an owner. `Scope::Reached` lists the recorded bindings the walk has declared
  (cleared by `HideLetBindings`, filled by `RevealLetBindings`); every recorded binding goes into the table through
  `Scope::DeclareLetBinding`, which notes it. `Scope::GetOwners()` is the table, then the reached bindings it no
  longer holds: what scope ends, exits, suspensions and loan release walk. Not every recorded binding: one not reached
  yet reads its state from `MemInfo`, which Stage 7 left initialised, so it would read as live. Codegen finds a frame
  owner by name and declaration position (`OwnedValue::DeclaredAt`), as two of one name can be owned.
- A shadowed binding keeps its value to its scope's end (affine since 2026-11-23, [[affine-ownership]]); the old
  `_CheckShadowed` (the hidden binding must be consumed) is gone. Its drop is found through `GetOwners()`.
- Code that needs the `let`'s target symbol while its value is checked uses `mem_utils::AssignmentTargetSymbol`
  (`meta->AssignmentTargetSymbol`), since the binding is not declared yet.

Failure signatures when a case is missed: segfault in `VariableSymbol::TypeRefIn` from a member access (`$tmp.field`,
`val.val`) - the name read a wrong or missing symbol; or a drop at an exit *before* the binding's line, as the
symbol reads as owned from the scope's start (E94 under the old linear rules). Trace with a temporary print in `HideLetBindings`/`_Declare`.
Tests: `TestShadowing.*`, `CaseExpressionAst.test_valid_nested_case_rebinding_the_subject_name`, the destructure groups,
`TestDropDestructure.test_valid_ret_before_*_destructure_of_a_temporary`.
