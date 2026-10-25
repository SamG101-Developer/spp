---
name: spp-meta-flags-are-per-stage
description: "a CompilerMetaData flag set in one stage is gone by the next; a keyword lowering that needs a licence in two stages must set it twice"
---

`CompilerMetaData` flags do **not** carry between stages. Each stage is its own traversal, and `MetaGuard` snapshots
on entry and restores on exit, so anything a stage sets is unwound before the next one runs.

This is easy to get wrong when copying the keyword-operator pattern.
`PostfixExpressionOperatorKeywordResAst` sets `IgnoreAccessModifierViolations` only in
`Stage7_AnalyseSemantics`, which reads as "the operator sets its licence once" — but that is only because
**visibility is checked in stage 7**. A licence needed by a stage-8 check has to be set in
`Stage8_CheckMemory` as well, over the forwarded `_MappedFn` call.

Seen 2026-09-09 adding the `await` keyword: it needs `IgnoreAccessModifierViolations` in stage 7 to reach the private
`Fut::await_`, and `ConsumptionJoinsEscapingBorrows` in stage 8, because moving a value that carries escaping borrows
is checked in `mem_utils::ValidateSymbolMemory` during memory analysis. Setting the second one in stage 7 alone does
nothing at all, silently.

Adding a keyword touches more than it looks like: the `RawTokenType` **and** `SppTokenType` enums plus
`TokToString`, a parse rule and its alternation entry, the ast, `ast_kind.ixx` (forward declaration, enumerator
placed so it stays inside its parent's `AstKindRange`), and `_all.ixx`. The **lexer needs no change** - it builds its
keyword table from the `RawTokenType` enumerator names via `magic_enum`, lowercasing whatever follows `KW_`.

And reserving a word breaks every existing identifier of that name. The convention already in the tree is a trailing
underscore: `sppc::async_` was named that because `async` is a keyword, and `sppc::await` had to become
`sppc::await_` for the same reason.

Related: [[spp-dollar-mock-types]], [[spp-linear-types]].
