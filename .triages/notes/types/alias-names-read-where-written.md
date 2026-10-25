---
name: alias-names-read-where-written
description: "Alias targets must be read/stamped in DeclScope (or the last alias that supplied the names), never the target class's LinkedScope; plus BoundAlias for bindings made before the target exists"
---

An alias's `LinkedScope` is its target class's own scope, where that class's parameters shadow names: `type Mine = Vec[A]` in `sup A`, read in `Vec`, is `Vec[A=A]`. So:
- `AliasTarget` and `KeyIn` read `DeclScope` first.
- `TypeStatementAst::Stage4` stamps in `_TargetWrittenScope`, from `RecursiveAliasSearch`'s 4th result. That is the last alias whose own `Written` supplied the names; a `use` alias that only carries the arguments on does not move it.

A binding to an alias whose target was not made yet (an instantiation during the alias stage) records `TypeSymbol::BoundAlias`. `FollowBoundAlias` re-links it on read, in `AsBoundSymbol` and in `FindParamBinding`, using pure lookups only. Without it, `type MyString = Str` gave `RawBuf[U8]` `T=SizedInteger` (the template) and codegen segfaulted.

**How to apply:** when an alias resolves "wrongly" only in some context, check which scope its written names are read in. Related: [[use-imports-are-alias-symbols]].
