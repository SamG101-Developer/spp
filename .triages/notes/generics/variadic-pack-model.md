---
name: variadic-pack-model
description: "Packs after the 2026-11 redesign: a function pack is a list of values (PackInfo, E123), a type pack a list of types (E124); templates reason about an open Tup[Ts], instances give each element a parameter; spreads, folds, comp packs, helpers"
updated: 2026-11-04
---

Three kinds of pack: a type pack (`..Ts`), a comp pack (`cmp ..n`) and a function pack (`..xs: Ts`, `..xs: T`,
`..xs: &Ts`). Since the redesign (`.triages/docs_open/pack-design.md`, phases 1-9, 2026-11-04) a function pack is not a
tuple and not a value; a comp pack still is a tuple (D5).

- **Markers.** `TypeSymbol::IsVariadic` for `..Ts`, `VariableSymbol::IsVariadic` for `cmp ..n`. A function pack is
  marked by `VariableSymbol::Pack` (`PackInfo`: `Element`, `Open`, `Elements`) alone, set in
  `FunctionParameterAst::Stage7`. Bindings copy them (`RegisterGnSymbols`, the symbol copy constructor).
- **A function pack is not a value (E123).** Only a spread (`..xs`), an element (`xs.N`), a collection (`(..xs)`) and a
  fold may name it whole; those set `AllowPackWhole` (`AllowPackWholeGuard`) while the identifier is analysed.
- **A type pack is not a type (E124).** Only the positions the parser marks (`TypeIdentifierAst::MarkPackPosition`)
  may name an unbound `Ts` written in source: a generic argument, a tuple shorthand's element, a variant's member
  (`Bool or Rest`), a variadic parameter's element type and the `..Ts` declaration. A meta flag set around analysis did
  not work: a parameter's type is analysed again elsewhere without it. A new list-like type syntax needs the mark in
  its parser rule, or its packs become E124.
- **In a template** a function pack is reasoned about as an **open tuple**, kept by design:
  - `..b: Ts` gives `b: Tup[Ts]`. A pack of one element type (`..xs: S32`, `..xs: T`) gets a **hidden type pack**,
    `Tup[VariadicPackOfxs]` (`packs::DeclareHiddenPack`, named by `packs::FnVariadicParamName`), which derives from
    the element (`DerivesFromSymbol`) and takes its constraints. The instance key binds it to the pack's
    convention-free tuple.
  - `type_compare::IsOpenTuple` recognises an open tuple by identity; `OpenTupleElement` types `b.N` (the derived type
    for a hidden pack). An element is typed from `PackInfo::Element` through its binding (`AsBound()`, so `Self` is
    the owner), never off the tuple. `UnboundPack` (member access) reads `PackInfo` first.
  - A constraint on `Tup[Ts]` is checked strictly: one every length meets holds, one only some meet (`IsPair`) fails
    (E51 with E78 inside).
- **In an instance** a function pack is **its elements**: `monomorphization::ExpandClosedPack` replaces the parameter
  with `xs#0`, `xs#1`, ... (`packs::PackElementName`), each with the pack's convention. The pack's symbol stays, closed
  (`PackInfo::Elements`), typed as the tuple so an out-of-range `xs.N` is E11, but owns nothing (`IsLive`). `..xs`
  expands to the element names (`ExpandTupleSpread`); `xs.N` is cloned as `xs#N` (`PackElementCloneGuard`). Codegen
  and mangling see ordinary parameters. A comp pack's instance value is still its tuple.
- **Calls.** `BindSlots` types a pack slot as the tuple of its arguments' types, conventions stripped, and inference
  unifies it with the parameter minus its convention (`..xs: &Ts` given `&a, &b` binds `Ts = (Str, S32)`). After
  instantiation `SplitPackSlot` gives each element its own slot: checked with its convention (an owned pack given `&a`
  is E51), ranked against the template's parameter, passed as its own argument.
  - An open spread in a template (`f(..b)`) is resolved once per length (`_ResolveOpenSpread`): calls with 0, 1, ...
    elements up to the most fixed parameters, then one with the open remainder, which only a variadic parameter takes
    whole. All must resolve to one return type, so head/tail recursion needs its empty overload. The remainder's slot
    is `OpenPackType`, conventions stripped too: keeping them typed a forwarded `..t: &Ts` as `Tup[&Ts]`, and `Ts`
    bound one element (the whole tuple).
  - Open forwards keep the tuple paths in `ElaborateCall` and `ValidateArgsMatchParams`, and a function with a
    variadic parameter left is `_IsPureGn`, never emitted (all kept by design).
  - A spread that is not the last positional argument is E122, the call's own error.
- **Spreads.** In a call or a tuple literal (`(a, ..t)`, a `SpreadExpressionAst`), via `expr_utils::ExpandTupleSpread`.
  A value that is no place (`..g()`) is hoisted into a `$spread` temporary, evaluated once (open-bugs #52). The written
  convention is given to every element (`&..t` is `&t.0, &t.1`); a plain `..t` moves out of an owned tuple, copies out
  of a borrowed one, and is E7 for a non-`Copy` element out of a borrow.
- **Folds.** `.. + xs`, `xs + ..` and `j(xs)..` fold a pack. A template checks a binary fold by induction (a fold of two
  and of three agree) and a call fold as one call on an element; an instance folds its element parameters
  (`BinaryExpressionAst::_FoldElements`, `_HandleFnFolding`). E125 is a spread in a fold, E126 packs of different
  lengths. Constraints: [[pack-constraints-pairwise]].
- **Comp packs** keep the tuple model: one element's type in a template (`..n` there is E19, #51), a tuple in an
  instance (`DoesCompNameAnUnboundPack`, `UnboundPack`'s `GnCompParam` branch).
- **Type packs as arguments.** `NameTypeArgs` spreads an argument naming a bound type pack into its elements
  (`A[S32, Ts]` with `Ts = Tup[Bool, U8]` is `A[S32, Bool, U8]`); an empty pack spreads into nothing. Sup attachment
  erases a block's pack binding, as the block stands for every element set. Making a type pack an argument list
  rather than a `Tup` is pack-design phase 11.
- **Helpers.** `packs::TypePackElements`, `CompPackElements` (and the `Bound...` forms), `DoesTypeNameAnUnboundPack`,
  `DoesCompNameAnUnboundPack`, `DoesArgNameAPack`, `DeclareHiddenPack`, `PackElementName`, `FnVariadicParamName`.

**Parser rule.** A closure's expression body must start on its parameter list's line (`_LineFeedAhead`, like a call's
`(`). Before this, `let t = ()` followed by a statement on the next line parsed as the closure `() <statement>`.

**Why:** the tuple model (2026-09-28, #14) let a pack be used whole, which needed band-aids for borrowed packs, nested
tuple naming and per-element checks; the redesign removed them.

**How to apply:** treat a function pack as a list in analysis code: ask `PackInfo`, never the pack's type. In a
template, the open tuple is how a pack is reasoned about, not a value.

Related: [[generic-inference-engine-shape]], [[pack-constraints-pairwise]], [[borrow-not-a-generic-argument]].
