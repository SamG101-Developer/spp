---
name: spp-destructure-alias-scope
description: "`x as y` is only legal inside an object destructure; the grammar has a split single-identifier rule to enforce it"
---

`LocalVariableSingleIdentifierAst::Alias` means "the name written is the *attribute's*, the alias is the binding" -
every reader of it does `Alias != nullptr ? Alias->Name : Name` to pick the symbol name. Only an **object**
destructure has an attribute name to rename. A tuple or array destructure binds by position, and a plain `let` or a
function parameter binds the name it was given, so `let x as y = 5`, `fun f(p as q: T)` and `let (a as b) = t` name an
attribute that does not exist.

The grammar used to allow it everywhere, because one rule carried the optional `as` and every context shared it -
which showed up as `fun hello(p` reporting `Expected ':', '::', 'as'`.

**How it is split** (`parser_spp.cpp`): `ParseLocalVariableSingleIdentifier` has no alias;
`ParseLocalVariableSingleIdentifierAliasable` calls it and attaches the optional `as` afterwards
(`p1->Alias = std::move(p2)`, the shape `ParseGlobalUseStatement` already uses for annotations). Only
`ParseLocalVariableNestedForDestructureObject` calls the aliasable one. The same split exists on the case side:
`ParseCaseExpressionPatternVariantSingleIdentifierAliasable`, used only by
`ParseCaseExpressionPatternNestedForDestructureObject`. Writing it as a wrapper rather than duplicating the
rule body matters on the case side, where the plain rule is itself two convention variants.

**Real users of the alias**, all object destructures - `single.spp` (`is Pass[NonNull[T]](val as p)`), `option.spp`
(`is Some[T](val as val1)`), and `tests/compiler/test_parse.cpp` (`let MyType(attr as a) = object`) plus
`test_ast_case_pattern_variant_destructure_object.cpp`. Verified after the split: `std` still builds and runs, the
object forms parse, and the four invalid forms are rejected.

**Multi-token operators name the whole operator, deliberately.** `::` is two raw `TK_COLON`s, `==` two
`TK_EQUALS_TO`s, `..` two periods - `ParseTokenDoubleColon` etc. call `ParseTokenRaw(raw, mapped)` once per raw
token, both times under the *whole* operator's `SppTokenType`. So a failure on the second half records
`TK_DOUBLE_COLON`, and the message can read `Expected '::'` at a position just after a `:` the author already typed.

That was changed once (a `TokenWantedHere(raw, mapped)` mapping the raw punctuation token to the `SppTokenType`
spelling that single character) and **The maintainer reverted it on 2026-09-07** - "go back to allowing :: after : in the error".
Naming the whole operator is the wanted behaviour: at the position where the operator is missing entirely it is the
useful thing to print, and that is the common case. Do not re-fix this.

Note `TokenTypeForMessage` in `parser_error_builder.ixx` must stay in the *named* `spp::parse::errors` namespace, not
an anonymous one: `SyntacticErrorBuilder::Raise` is an exported template, and gcc rejects a module exposing a TU-local
entity ("exposes TU-local entity").
