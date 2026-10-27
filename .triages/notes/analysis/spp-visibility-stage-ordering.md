---
name: spp-visibility-stage-ordering
description: "Attribute visibility is synced onto the symbol in Stage4, not Stage5, because Stage5 is when definitions read each other"
---

A `!public` attribute read through a **destructuring function parameter** was reported as `private` (E83), while the
identical destructure in a function *body* compiled. Not a grammar problem and not a rule about parameters: a
same-stage ordering race.

**The chain.** `FunctionPrototypeAst::Stage5_LoadSupScopes` calls `FnParamGroup->Stage7_AnalyseSemantics` - the
comment says it is there to analyse parameter *types*, but it analyses whole parameters, and a destructuring one
expands into `PostfixExpressionOperatorRuntimeMemberAccessAst` nodes that check attribute visibility as they are
built. `ClassAttributeAst::Stage5_LoadSupScopes` was where the symbol's `Visibility` got synced from the AST (the
symbol is created in Stage2 with the *default*, private). Both in Stage5, so whether `!public` had landed came down
to which definition the walk reached first.

**The fix**: apply visibility a stage earlier, where nothing reads it yet. `AnnotationAst::Stage4_QualifyTypes`
already resolves the annotation's fq name (`FindVarSymbolOutermost` - no `Stage7_AnalyseSemantics` needed, unlike the
Stage5 path), so a `VisibilityOf(fq_name)` helper now maps the name to the visibility in Stage4 *and* Stage5 (one
table, both call sites), and `ClassAttributeAst::Stage4_QualifyTypes` syncs it onto the symbol. Stage4 completes for
every module before Stage5 begins for any, so the race is gone.

**How to apply:** verify with a matrix, not one case - public/private attribute x parameter/body position, plus the
`sup` case where private is legitimately reachable. Private must still be *rejected* through a destructured
parameter; a "fix" that only makes the error go away has removed the check.

**Type names (2026-10-24, open bug 45):** a class's visibility reaches its *type symbol* only in
`ClassPrototypeAst::Stage5`, so `TypeIdentifierAst` cannot check a name read in stages 4-5 (signature and attribute
types). Such a name records a deferred check (`_PendingVisibility`), run by `CheckPendingVisibility` at the start of
stage 6. Generic parameters are not recorded. Two-module cases live in `tests/codegen/check-visibility.sh` (gtests are
one module; std has no private class).

**Dead end:** moving `FnParamGroup->Stage7_AnalyseSemantics` from prototype Stage5 to Stage7 (which is where binding
parameter names belongs, and it is the *only* call site) segfaults the compiler - something between Stage5 and Stage7
needs the parameter symbols. Don't retry without finding that reader first. See [[spp-hasanalysed-conflation]] for the
related order-dependence trap.
