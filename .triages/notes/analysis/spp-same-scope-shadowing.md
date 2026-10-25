---
name: spp-same-scope-shadowing
description: "How a name means the right binding after Stage 7: lets declare at their position in every walk (M2, 2026-10-10)"
updated: 2026-10-10
---

A scope's symbol table holds one symbol per name (`Add` replaces), so after Stage 7 a name belongs to the *last*
binding declared in the scope. Until 2026-10-10 that was patched per-ast (`_PrevSymbol`/`_ExposePreviousSymbol`/
`_UsesSavedSymbol`, a `PosStart` check in `CheckLiveUpTo*`, an init take-down). M2 of
`.triages/docs_closed/memory-analysis-refactor.md` replaced all of it with one model:

- Stage 7 records each `let`'s binding on its scope (`Scope::RecordLetBinding`). `MoveToNextScope` hides them
  (`HideLetBindings`); each `let` redeclares its own once its value is done (`LocalVariableSingleIdentifierAst::_Declare`),
  in Stages 8, 9 and 11 and in every temporary `ScopeManager`.
- Leaving or skipping a scope reveals them, last one winning (`MoveOutOfCurrentScope`, `SkipPastScope`, `ExhaustScope`):
  outside a walk a scope reads as Stage 7 left it. Needed because a value's type is inferred before it is walked.
  Skip loops must use `SkipPastScope`, never `while (..) MoveToNextScope(false)` (that hides without revealing).
- Anything that adds a symbol directly (not via a `let`) that a later `let` of the same name replaced must redeclare it
  in every walk at its position: case-pattern flow symbols (`_DeclareFlowSymbol`), destructure cond lets (`Narrow`).
  Bindings declared up front by an owner (params, closure captures) are `UnrecordLetBinding`-ed.
- Code that needs the `let`'s target symbol while its value is checked uses `mem_utils::AssignmentTargetSymbol`
  (`meta->AssignmentTargetSymbol`), since the binding is not declared yet.

Failure signature when a case is missed: segfault in `VariableSymbol::TypeRefIn` from a member access (`$tmp.field`,
`val.val`) - the name read a wrong or missing symbol. Trace with a temporary print in `HideLetBindings`/`_Declare`.
Tests: `TestShadowing.*`, `CaseExpressionAst.test_valid_nested_case_rebinding_the_subject_name`, the destructure groups.
