---
name: type-source-span-override
description: "A type rebuilt from its symbol must be stamped with WithSourceSpanOf(written), or errors point at the declaration/generated code; the SHOULD_FAIL macros now check error locations"
updated: 2026-09-14
---

Since 2026-09-14, any code that replaces a written type with a rebuilt one (`FqName()`, `SubstituteSelfType*`, a bound generic, an alias target) must end the expression with `->WithSourceSpanOf(*written)`. The rebuilt type then reports the written span (`TypeAst::_HasSourceSpan`) and keeps the written text for "Written (aka Qualified)" messages (`TypeForMessage` in semantic_error.cpp). `FqName()` is cached and shared, so `WithSourceSpanOf` clones - never stamp in place.

**Why:** a rebuilt type carries its declaration's positions (or 0), so errors landed in std, "at the end of the file", or "<generated code>", and underlines overran by the length difference between written and qualified names. `WithConvention` on a stamped type must keep the span too - the namespace-node `WithConvention` built new nodes without it, which broke the case-pattern destructure once its `Source.OriginalType` fallback was removed.

**How to apply:**
- New rewrite site: stamp last, after any `WithConvention`/`WithGns`.
- `SPP_TEST_SHOULD_FAIL_SEMANTIC` now fails on generated/EOF blocks and on a primary block outside main.spp; `SPP_TEST_SHOULD_FAIL_SEMANTIC_AT(..., "underline", ...)` also checks the underlined text. Of the 61 tests that were red on this, 60 pass since 2026-09-14; the rest is E71 (a type inferred off a literal has no position; stamping would clone a type per argument in overload resolution).

Where a bad block comes from, found fixing those 61:
- `<generated code>` = a node at position 0: a token defaulted by `SPP_SET_AST_TO_DEFAULT_IF_NULLPTR` without a position (closure `Tok`, member-access `TokDot`, case-pattern `ConvToVar` destructures), or a type rebuilt without a stamp (constraints, generic defaults, coroutine `EnclosingFnRetType`). `SemanticError::AddErr` narrows a postfix expression to its operator (`UnwrapFunctionCallAst`), so an operator's own tokens need positions, not just its operand's.
- `<end of file>` = a position past the author's code, i.e. in the prelude the lexer appends to every file; `Scope::IsFromPrelude` tells, and a prelude re-import raises the one-block `SppIdentifierDuplicateError(ast, what)`. It is also what std positions look like when formatted against a main.spp scope: tokens cloned from a std prototype (a `self` convention, a parameter name made into a keyword argument) must be re-placed with `PatchPos`/a new identifier.
- A block in std for user code = comp-time evaluation; nested calls report at `meta->CompTimeCallSite`, the outermost call.
- Scope order matters: `RaiseIf` pairs scopes with the source-quoting blocks (errors and contexts) in order, cycling when fewer. Until 2026-09-14 `SemanticErrorBuilder::Raise` cycled formatters over *every* entry, header and footer included, so every two-scope error had its scopes swapped - invisible when both are in one file, "<end of file>" plus a primary block in std when they are not. Pass the declaring scope for a node declared elsewhere (`decl_scope` on `EnforceGenericConstraintsAllArgs`); a generic instance's scope already renders in the template's file (its parent is the template's). Fixing the builder exposed sites written against the swap (E81 annotation sites, E83 visibility, abstract type use, double extension, ext type statement, the sup-conflict duplicates in `scope_manager.cpp`); every raise with 2+ distinct scopes was audited against its ctor's `AddCtxForErr`/`AddErr` order on 2026-09-14 - a new raise lists its scopes in that same order.
- Error locations are deterministic since `IdentifierAst::Clone` copies `_ForTok` (uninitialised before, so `PosEnd` was garbage and 66 tests pointed differently run-to-run). If they drift again, suspect an uncopied position field first.
- Remaining `Source.Original*` fields are semantic (FunctionParameter/ClassAttribute/ObjectInitializer `OriginalType`, `OriginalOldType`, `OriginalImpl`, ...) - don't remove them for position reasons.
