---
name: folded-template-body-reads-bindings
description: "A cmp call folds the template's body, not the instantiation's: nested calls read Self and type args through the folded call's template bindings"
updated: 2026-11-17
---

`PostfixExpressionOperatorFunctionCallAst::EvaluateCompTime` runs `fn_proto->Impl` where `fn_proto` is
`GetNonGnImpl()`, the template. A call inside that body was analysed in the template's scope, so its `SelfId` and type
arguments are in the template's terms: `Self::zero()` in `sup [cmp w: U32] SizedFloatingPoint[w]` names
`SizedFloatingPoint[w]`, whose `w` has no value, and a builtin reading the width (`SizedNumericLiteralTypeName`) ICEd
("no numeric bounds for this type", open-bugs #64). Since 2026-11-17 the folding call unifies the template's `Self`
(`FindEnclosingSelfType` of the template scope) with its concrete `Self` (`UnifyTypeIds`) and keeps the bindings in a
thread-local for the duration of the body; each nested call substitutes its `SelfId` and type arguments with them
(`SubstituteTypeId`). The previous fold's bindings are restored on the way out.

**Why:** `F64::zero()` folded fine (the call names `F64` directly); only a call reaching a builtin through a generic
body (`F64::nan()` -> `Self::zero()`) saw the template's `Self`.

**How to apply:** a new comp-time builtin that reads a type off `GnTypeArgs` gets the concrete type through this; a
nested call whose types come from somewhere else than `SelfId`/type args (a parameter's declared type, say) is still in
template terms. Related: [[evaluators-only-read-their-ast]], [[stage-9-walk-removed]].
