---
name: variadic-pack-model
description: "Packs after the 2026-11 redesign: every pack (type, comp, function) is a list, never a tuple and never named; a function or comp pack is no value (E123), a type pack no type (E124); tuples and variants are classes over a pack; helpers"
updated: 2026-11-05
---

Three kinds of pack: a type pack (`..Ts`), a comp pack (`cmp ..n`) and a function pack (`..xs: Ts`, `..xs: T`,
`..xs: &Ts`). Since `../../docs_closed/pack-design.md` (phases 1-11, 2026-11-04) each is a **list**: no pack is bound
to a tuple, and a tuple or a variant is just a class over a pack (`cls Tup[..Items]`, `cls Var[..Variants]`).

- **Identity.** A type pack's binding is a `TypeKey` headed `Pack` (`scopes::PackIdOf`, `IsPackTypeId`,
  `TypeKeyHeader::PackElements`); a comp pack's a `CompKey` of `Part::Pack`. A tuple value in a comp argument is
  `Part::Tuple`, so the two never meet. An instance files a pack under its parameter like any argument
  (`Tup[S32, Bool]` is `Inst(Tup, Items = Pack(S32, Bool))`), always present, empty too. A class of only packs, all
  empty, is keyed as its template (`InstanceIdOfArgs`): `()` is `Tup`, so the empty instance has one identity.
- **No pack is named (maintainer).** The variadic parameter is last and takes the trailing positionals; `Ts=...` or
  `ns=...` is E46. Positionals may follow keywords only as that tail, as in Python (`P[A=S32, Bool, U8]`,
  `f(a=1, ..xs)`); one landing on a fixed parameter is E18 (`NameArgs`, `BindSlots`; `DoOrderArgs` leaves the
  tail). In the AST a pack's elements stay positional after the named arguments, and are written back that way
  (`TypeAstOf`, the solver's `TakeArgs`, `FromParams`, mangling).
- **Binding.** A type pack's binding symbol holds its list (`TypeSymbol::BoundPack`, no scope; `KeyBinding` keys it as
  the list); a comp pack's keeps its list identity (`BoundCompId`) while an instance binds its value to its tuple.
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
  An open tuple is a `Tup` whose list ends in an unbound pack (`IsOpenTuple`, reading `IdArgs`). A comp pack there
  (`cmp ..ns: Bool`) is declared the same way, at the parameter's stage 7 (`GenericParameterAst`): typed
  `Tup[CompPackOfns]` with `PackInfo{Element}`, so spreads, `ns.0`, forwards and folds take the function pack's path,
  and used whole it is E123 as a function pack is (D5 superseded 2026-11-05; `(..ns)` is its tuple). Named alone as a
  comp generic argument it is forwarded (`AnalyseCompExpr` allows the pack whole there). A forward as a comp argument
  (`A[ns]`) is checked by the element type (`_CheckCompArgs`), and an instance reads the declared element off
  `PackInfo` (`CreateGnCompSymbol`). An open spread stands for every length (`_ResolveOpenSpread`), so only a variadic
  callee takes it: fixed parameters alone are E51 (#51, N14.1, 2026-11-04).
- **Calls.** A pack slot holds a list, not a tuple (`ParamSlot::Pack`, a `packs::GivenPack`: its elements' types,
  conventions stripped, then an open spread's own list, which leaves it `Open`). `InferAllGns` unifies the list
  against `Ts` by identity (`GenericSolver::UnifyPack`), element by element for a pack of one type. The list keys the
  instance (`CallArgsId`) and becomes its parameters (`ExpandClosedPack`); a tuple of it is built once, as the
  instance's pack symbol, and once for an open forward checked whole (`ValidateArgsMatchParams`). N14.3, 2026-11-04.
  In an instance a closed pack is its elements (`ExpandClosedPack`, `xs#0`, ...); see also [[pack-constraints-pairwise]].
- **Variants** are their positional members (`Var[S32, Bool]`), normalised, flattened and ordered where written
  (`TypeIdentifierAst`) and keyed as members (`VariantKey`). Reading a written type's members off its arguments
  (`type_members::VariantMembers`) is only for `Var` itself: check the written name's own symbol
  (`FindHeadSymbol(...)->UseTarget()->Alias`), not `ForKindCheck`, which resolves an alias (`Opt[T=Opt[S32]]`) to `Var`
  and made its `T` look like members (a refined pattern then bound the inner tag, 2026-11-04).
- **Spreads and folds**: `expr_utils::ExpandTupleSpread` (a temporary hoisted once, #52); binary and call folds over a
  function pack (E125 spread in a fold, E126 lengths differ). A call fold is the tuple of its calls' results, or
  `Void` when the callee returns `Void` (`_FoldIsVoid`), so `drop(xs)..` is a statement.
- **Helpers.** `packs::GivenPack`, `CompPackElements`, `BoundCompPackElements`, `DoesTypeNameAnUnboundPack` (a
  pack bound to a list counts as bound), `DeclareHiddenPack`, `PackElementName`, `FnVariadicParamName`;
  `TupleLiteralAst::ElementTypes` (what a tuple of some values holds, a spread of an open tuple as its elements).

**Parser rule.** A closure's expression body must start on its parameter list's line (`_LineFeedAhead`, like a call's
`(`). Before this, `let t = ()` followed by a statement on the next line parsed as the closure `() <statement>`.

**Why:** a pack bound to a tuple made `Tup`'s own pack recursive (`Tup[Items=Tup[...]]`), so tuples were special-cased
everywhere (positional keys, no bindings, no solver), and the comp side needed side lists to tell a pack from a
tuple value. As lists, every class over a pack is handled alike.

**How to apply:** ask the identity whether something is a pack (`IsPackTypeId`, `Part::Pack`), never a side list or a
`Tup`. A new list-like type syntax needs `MarkPackPosition` in its parser rule. Anything that reads an instance's
arguments as a flat list should go through `IdArgs`/`TypeArgRefs` (lists spliced).

Related: [[generic-inference-engine-shape]], [[pack-constraints-pairwise]], [[borrow-not-a-generic-argument]].
