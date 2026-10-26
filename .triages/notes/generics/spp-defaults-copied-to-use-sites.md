---
name: spp-defaults-copied-to-use-sites
description: "Parameter/attribute defaults are copied into every use site - why they may not create scopes, and when/where they are analysed"
---

A parameter default is cloned into every call that leaves the argument out (`arg_naming::NameFnArgs`), and an
attribute default into every object initializer that leaves the attribute out
(`ObjectInitializerArgumentGroupAst`). Everything below follows from that (settled 2026-09-10).

**What a default may be:** `Ast::IsAllowedInDefault()` - a deny-by-default virtual (base returns false), overridden
by leaves and composites; failures raise `SppInvalidDefaultValueError` (E108), checked on the *written* default.
Refused: `ret`/`?` (would leave the caller), and **anything that creates a scope - blocks, closures, `case`, `loop`,
`gen`**. Scopes are matched to asts by walk order (one pre-order iterator; `CreateAndMoveIntoNewScope` does `++_It`),
so a copy analysed at a use site has no scope of its own in that walk. The maintainer chose refusing them over a
wrap-and-re-parent redesign; no std/project code used them. `cmp` statements need no such check - their fold (Stage 7 since
2026-10-26, Stage 9 before) is already a deny-by-default compile-time check.

**When parameter defaults are analysed:** in `FunctionPrototypeAst::AnalysePendingDefaults`, drained by
`CompilerBoot::Stage6` after every module's Stage 6. Not in Stage 7 (caller bodies can precede the callee, which handed
out an unresolved call - the `_OInfo` abort). Not in each prototype's own Stage 6 (a default can instantiate a generic,
e.g. `n + 1` -> integer `add`, whose template substitutes `Self` only in *its* Stage 6 - E26 in std's `sadd`).
Instantiations still analyse theirs in Stage 7 (`_DefaultAnalysed` guards the double visit).

**Generic callees** substitute from `Source.OriginalDefaultVal` (analysis moves a binary operator's operands into its
lowered call) and analyse the copy in the callee's scope by swapping `CurrentScope` on the existing manager, as
attribute defaults do - never a fresh `ScopeManager`.

**`cmp` generic defaults** are folded via Stage 9 where the bound value is emitted (`CmpStatementAst::Stage11`,
constant context), mirroring `StaticMemberAccessAst`. Not at binding time: that is Stage 7, before any `cmp` constant
has been folded. Related: [[spp-comp-generic-default-crash]], [[spp-self-type-resolution]].

**Open bug (found 2026-09-22): a non-generic default is re-resolved in the *caller's* scope.** `InferAllGns`
calls `InferType` on the cloned default at the call site, and `StaticMemberAccessAst` re-resolves its lhs from
`sm->CurrentScope` every time (no cached ns symbol). std's `sleep(.., clock: S32 = cffi::clock_realtime)` called from
outside std -> `ConvertPostfixToNestedScope("cffi")` is null -> segfault in `FindVarSymbol`. Passing the arg explicitly
avoids it. Any default naming something the caller cannot see (unimported type, module-relative path) has the same shape.
