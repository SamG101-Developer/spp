---
name: stamps-live-on-last-part
description: "A type's stamp must be on its LastTypePart (KeyStamped reads only that); FqName and the precompiled common types now stamp there (2026-10-07), and why a missing stamp is a silent slow path"
---

`KeyStamped` (in scope.cpp `RawTypeKey`) reads only `t.LastTypePart()->StampedTypeId()`. Every stamper must stamp
the last part: `StampTypeParts`, `TypeAstOf` and now `FqName` do. Since 2026-10-07 `FqName` stamps both the last part
(a copy of `Name`, because `Name` is shared) and the outer node (`FindTypeSymbol` reads that first).

A stamp on the wrong node is never an error. It just falls back to a by-name lookup in `RawTypeKey` and still finds
the right symbol. Probes on a std compile before the fixes:
- 167k type keyings were stamped only on the outer node (`FqName`).
- 86k came from generated concrete std types.

Generated concrete std types:
- The precompiled constants (`BOOL`, `S32`, `STR_VIEW`...) are stamped once per compile by
  `type_resolution::StampPrecompiledTypes`, which runs right after Stage 3 in `Compiler::Compile`.
- Fresh ones go through `common_types_precompiled::BoolAt/VoidAt/NeverAt/StrViewAt(pos)`, which copy that stamp.

**Why:** don't use `WithSourceSpanAt(*this)` to place a cloned type at a statement. It calls `PosEnd()`, which
crashes on ASTs whose parts were moved out by desugaring (`GenWithExpressionAst::Expr` → `_MappedLoop`; binary, index
and gen do the same).

**How to apply:** any new code building a concrete common type should use the `…At(pos)` helpers. To find what is
still unstamped, count `ByName` exits in `RawTypeKey` with an empty last-part stamp.

Left unstamped (~28k per std compile):
- variant `Tup[...]` member lists
- mocks (`$Map`)
- short source names
- unmade instances
