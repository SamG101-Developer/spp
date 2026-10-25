---
name: spp-ast-reflection-refactor
description: "Reflection-generated PosStart/PosEnd/Clone/ToString was tried 2026-09-18 and REVERTED; the latent ast bugs it found, and the lessons for a retry"
updated: 2026-09-18
---

On 2026-09-18 (branch `async`) the four key ast functions were generated from C++26 reflection
(engine + markers + macros under `headers/spp/asts/reflect/`, one `SPP_AST_KEY_FUNCTIONS_IMPL` per
`.cpp`). 109 of 134 nodes were converted, then the user **reverted it "for now, for the future"**.
None of that code is in the tree. `.triages/docs_open/ast-reflection-plan.md` records the design if it is retried.

**Latent bugs it found, all still present at HEAD, all independent of reflection:**
- Five nodes segfault if asked for a position when empty: `GenericArgumentGroupAst`,
  `GenericParameterGroupAst`, `FunctionParameterGroupAst` (null `TokL`/`TokR`),
  `ClosureExpressionCaptureGroupAst` (null `TokCaps`, `Captures.Back()` on empty),
  `GenericParameterTypeInlineConstraintsAst` (null `TokColon`). Nothing asks today; an error
  underlining one would crash.
- `LocalVariableSingleIdentifierAliasAst::ToString` emits a NUL: `.append_range(" ")` appends the
  `char[2]` terminator too (398 NULs over `project/`). Only `append_range` in the codebase; the
  string reaches scope block names and mangling.
- Span oddities that look unintended: variadic `..Ts` ends at the `..` (GenericParameterAst::PosEnd
  checks TokEllipsis before Name); `LocalVariableDestructureObjectAst::PosStart` skips `Type`;
  `String/CharLiteralAst::PosStart` skip `BytePrefix`; `FunctionParameterSelfAst::PosEnd` is
  `PosStart() + 2`.

**Lessons for a retry.** The suite fails 47/2378 at `b9b9054d` before any change — compare failure
*lists* against a baseline. A parsed-tree dump of PosStart/PosEnd/ToString cannot see `Clone`,
analysis-built nodes with null tokens, or lowered-node paths; the three full-red regressions were
all there. Diff every old `Clone` against the engine field-by-field, both directions. A bisect that
reverts node files but not shared engine rules proves nothing. An unexplained `_OverloadInfo`
disengaged crash (1568 failures) was never root-caused before the revert.

Related: [[split-macro-headers]], [[ast-clone-carries-cached-analysis]], [[add-tests-even-if-failing]].
