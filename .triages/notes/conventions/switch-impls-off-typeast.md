---
name: switch-impls-off-typeast
description: "The analysis/codegen implementations are to move off TypeAst onto symbols/TypeRef; bigger rewrites are wanted when they reduce code"
updated: 2026-09-15
---

Move the implementations themselves off `TypeAst` onto `TypeSymbol`/`TypeRef` (comparison, inference, coercion,
variant/argument readers), not just add ref overloads beside AST ones. Bigger rewrites are explicitly wanted for this,
since they also reduce code; keep `TypeAst` only where syntax is genuinely built or rewritten (substitution, spelling).

**Why:** (2026-09-15) asked "all four phases" of the TypeRef migration, including rebuilding `TypeEq`'s core on refs,
with the suite still skipped: "we are trying to switch the impls away from TypeAst so some bigger rewrites for this is
definitely needed as it reduces the code too".

**How to apply:** prefer replacing an AST path with a ref path and deleting the AST one over adding a parallel ref
overload. Verify each step by recursive IR diff ([[spp-ir-diff-verification]]); a parallel ref path that leaves the AST
one in place is not progress. Land a risky one by running both and logging every disagreement until only intended ones
remain (this is how `TypeEq` and `RelaxedTypeEq` went in). See [[resolved-types-generic-identity]].

**Where `TypeAst` is the right answer** (2026-09-16, each checked): a *pattern* (a `sup` block's or function's written
generic shape) - it is syntax written over that block's own parameters; anything building syntax (an instantiation's
name, a deduplicated variant's `Tup`, a prototype built from `Args`/`Out`); error text; and a type stored on a symbol
(`VariableSymbol::Type`, `IndirectFnType`). Converting those adds a written copy beside the ref instead of removing
one. The maintainer: "sometimes we can add lines - corrections etc, its just preferable it gets reduced".

**Phase 1 done (2026-10-08), see .triages/docs_closed/typeast-migration-plan.md.** `InferTypeRef` is pure virtual and primary;
`InferType` stays a virtual *display hook* (the maintainer chose this over deleting it: a ref can't carry an alias name like
`S32` or a source span). Its default is `InferTypeRef().AstIn()` placed at the expression via `WithSourceSpanAt`.
Don't delete forwarder/literal/identifier `InferType` overrides: they keep alias names in messages.
