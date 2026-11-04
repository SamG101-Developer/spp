---
name: variadic-pack-model
description: "Packs after the 2026-11 redesign: every pack (type, comp, function) is a list, never a tuple and never named; a function pack is no value (E123), a type pack no type (E124); tuples and variants are classes over a pack; helpers"
updated: 2026-11-04
---

Three kinds of pack: a type pack (`..Ts`), a comp pack (`cmp ..n`) and a function pack (`..xs: Ts`, `..xs: T`,
`..xs: &Ts`). Since `../../docs_closed/pack-design.md` (phases 1-11, 2026-11-04) each is a **list**: no pack is bound
to a tuple, and a tuple or a variant is just a class over a pack (`cls Tup[..Items]`, `cls Var[..Variants]`).

- **Identity.** A type pack's binding is a `TypeKey` headed `Pack` (`scopes::PackIdOf`, `IsPackId`,
  `TypeIdHead::PackElements`); a comp pack's a `CompKey` of `Part::Pack`. A tuple value in a comp argument is
  `Part::Tuple`, so the two never meet. An instance files a pack under its parameter like any argument
  (`Tup[S32, Bool]` is `Inst(Tup, Items = Pack(S32, Bool))`), always present, empty too. A class of only packs, all
  empty, is keyed as its template (`InstanceIdOfArgs`): `()` is `Tup`, so the empty instance has one identity.
- **No pack is named (maintainer).** The variadic parameter is last and takes the trailing positionals; `Ts=...` or
  `ns=...` is E46. Positionals may follow keywords only as that tail, as in Python (`P[A=S32, Bool, U8]`,
  `f(a=1, ..xs)`); one landing on a fixed parameter is E18 (`NameArgs`, `BindSlots`; `DoOrderArgs` leaves the
  tail). In the AST a pack's elements stay positional after the named arguments, and are written back that way
  (`TypeAstOf`, the solver's `TakeArgs`, `FromParams`, mangling).
- **Binding.** A type pack's binding symbol holds its list (`TypeSymbol::BoundPack`, no scope; `KeyBinding` keys it as
  the list); a comp pack's keeps its list identity (`BoundCompId`) while its value is spelled as its tuple (D5).
  `GenericSubst` is only `TypeParams` and `CompParams`: a binding that is a list is a pack. `BindType`/`BindComp`
  bind a pack to a list and nothing else.
- **Splicing.** An element naming a bound pack is spliced into the list it is in, wherever a list is keyed or
  substituted (`Scope::ArgsIdOf`, `PackIdOfElements`, `RewriteTypeId`/`RewriteCompId` on a `Pack`), so `Tup[A, Ts]`
  with `Ts` bound to `B, C` is `Tup[A, B, C]`. Reading an instance's arguments (`IdArgs`, `TypeArgRefs`) splices its
  lists into elements, which is what tuple layout, destructuring and assignability see.
- **Unification.** Two lists match element by element, a trailing pack in the pattern taking the rest
  (`UnifyType`'s `Pack` case, `UnifyCompIds`). A variant pattern's pack takes the members the others leave.
- **A function pack is no value (E123)**, only spread, indexed, collected (`(..xs)`) or folded (`AllowPackWhole`).
  **A type pack is no type (E124)** outside the positions the parser marks (`MarkPackPosition`; checked once with
  the symbol the main path found, kept as `_TypePack` for the early-return paths).
- **In a template** a function pack is reasoned about as an open tuple: `..b: Ts` is `Tup[Ts]`; one of a single
  element type gets a hidden pack (`VariadicPackOf<xs>`, `packs::DeclareHiddenPack`), deriving from the element.
  An open tuple is a `Tup` whose list ends in an unbound pack (`IsOpenTuple`, reading `IdArgs`).
- **Calls.** A pack slot is typed as the tuple of its arguments (conventions stripped; `OpenPackType` for a forward).
  `InferAllGns` unifies it against `Tup[Ts]` for a type pack, element by element for one of a single type;
  `ValidateArgsMatchParams` expects `Tup[Ts]`. The instance's hidden pack is bound to the slot's list (`CallArgsId`).
  In an instance a closed pack is its elements (`ExpandClosedPack`, `xs#0`, ...); see also [[pack-constraints-pairwise]].
- **Variants** are their positional members (`Var[S32, Bool]`), normalised, flattened and ordered where written
  (`TypeIdentifierAst`) and keyed as members (`VariantKey`). Reading a written type's members off its arguments
  (`type_members::VariantMembers`) is only for `Var` itself: check the written name's own symbol
  (`FindHeadSymbol(...)->UseTarget()->Alias`), not `ForKindCheck`, which resolves an alias (`Opt[T=Opt[S32]]`) to `Var`
  and made its `T` look like members (a refined pattern then bound the inner tag, 2026-11-04).
- **Spreads and folds**: `expr_utils::ExpandTupleSpread` (a temporary hoisted once, #52); binary and call folds over a
  function pack (E125 spread in a fold, E126 lengths differ). A call fold is the tuple of its calls' results, or
  `Void` when the callee returns `Void` (`_FoldIsVoid`), so `drop(xs)..` is a statement.
- **Helpers.** `packs::TupleTypeElements` (a tuple type's written elements), `CompPackElements`,
  `BoundCompPackElements`, `DoesTypeNameAnUnboundPack` (a pack bound to a list counts as bound),
  `DoesCompNameAnUnboundPack`, `DeclareHiddenPack`, `PackElementName`, `FnVariadicParamName`.

**Parser rule.** A closure's expression body must start on its parameter list's line (`_LineFeedAhead`, like a call's
`(`). Before this, `let t = ()` followed by a statement on the next line parsed as the closure `() <statement>`.

**Why:** a pack bound to a tuple made `Tup`'s own pack recursive (`Tup[Items=Tup[...]]`), so tuples were special-cased
everywhere (positional keys, no bindings, no solver), and the comp side needed side lists to tell a pack from a
tuple value. As lists, every class over a pack is handled alike.

**How to apply:** ask the identity whether something is a pack (`IsPackId`, `Part::Pack`), never a side list or a
`Tup`. A new list-like type syntax needs `MarkPackPosition` in its parser rule. Anything that reads an instance's
arguments as a flat list should go through `IdArgs`/`TypeArgRefs` (lists spliced).

Related: [[generic-inference-engine-shape]], [[pack-constraints-pairwise]], [[borrow-not-a-generic-argument]].
