---
name: error-scope-per-block
description: "Since N8.7 (2026-10-17) an error quotes each AST through the file its position names (ErrorFormatter::Owning); Raise takes no scopes at all"
---

Since N8.7 (2026-10-17) every source file's formatter is made with `ErrorFormatter::ForFile`, which gives the file its
own position range (`PositionOf(i)` = base + token index, bases `(k+1) << 24`). The parser stamps those positions, so
any AST (clones and generic instances included, which keep their template's positions) names its file, and
`ErrorFormatter::Owning(ast)` finds it. `SemanticErrorBuilder::Raise`, `spp::Ice`, the resolution index spans and
`Scope::IsFromPrelude` all quote through the owning file.

Later the same day the scopes argument went: `Raise<E>(ERR_ARGS(...))`, `RaiseIf<E>(cond, ERR_ARGS(...))`. An AST no
file owns is written out (`WithoutSource`) and its JSON label filed under the error's first owned block. `ErrTypeAt(ref,
at, scope)` names its type in the scope it is given.

**Why:** before, `Raise<E>({scopes...}, ...)` paired each quoted block with the scope at the same index (cycling), so a
block from another file than its paired scope rendered "at the end of the file" or the wrong line. A probe showed the
fallback only ever added a file name to generated blocks, nearly all in discarded overload-trial errors.

**How to apply:**
- Never add a scope parameter just so an error can be raised from it; if a block shows "In generated code", the fix
  is to give that AST a position (see N8.8 in .triages/next-steps.md), not to pass a scope.
- Never index a formatter's tokens with a raw position: convert through the formatter (`_TokenIndexOf`). Tests that
  build formatters: `tests/compiler/test_msc_error_files.cpp`; std-from-main.spp quote cases stay in
  `test_msc_error_locations.cpp`.
