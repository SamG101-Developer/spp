---
name: variadic-pack-model
description: "One model for all three pack kinds (type ..Ts, comp cmp ..n, function param ..xs): IsVariadic on the symbol; in a template an open tuple (Tup[Ts]) or one element, a tuple in an instance; spreads resolved per length; the shared helpers; the closure-newline parser rule"
updated: 2026-11-02
---

Since 2026-09-28 (#14), packs share one model:

- **Marker.** The symbol carries `IsVariadic`: `TypeSymbol` for `..Ts`, `VariableSymbol` for `cmp ..n` and for a
  function parameter `..xs` (set in `FunctionParameterAst::Stage7`). Bindings copy it (`RegisterGnSymbols`, the
  symbol copy constructor).
- **Template vs instance.**
  - In a template a pack over a type pack is, as a whole, an **open tuple**: `..b: Ts` gives `b: Tup[Ts]`
    (`FunctionParameterAst::Stage7`, since 2026-11-02). `type_compare::IsOpenTuple` recognises one by identity (a
    tuple whose last argument is an unbound variadic parameter), and `OpenTupleElement` types `b.N` as `Ts`. A
    constraint is checked against `Tup[Ts]` strictly: one every tuple length meets holds (`sup [..Us] Tup[Us] ext X`),
    one only some lengths meet (`IsPair`) fails in the template (E51 with E78 inside).
  - Any other pack in a template keeps **one element's** type (`S32` for `..xs: S32`, `T` for `..xs: T`, `Bool` for
    `..n: Bool`), as no type spells "a tuple of T's of unknown length".
  - In an instance it is the **tuple** (`RetypeInheritedSymbols` / the comp arg's value type).
- **Spreading an open tuple** (`f(..b)` in a template) is left unexpanded by the argument group, and the call is
  resolved once per pack length instead (`PostfixExpressionOperatorFunctionCallAst::_ResolveOpenSpread`): a call with
  0, 1, ... `b.N` arguments up to the most fixed parameters any candidate has, then one with that many and the open
  remainder, which only a variadic parameter takes whole (`CheckCandidate` rejects anything else; `BindSlots` and
  `ElaborateCall` pass it on as the pack). All must resolve, to one return type. So head/tail recursion needs its
  empty-pack overload, and forwarding `..b` into another variadic works. Instances expand the spread as usual,
  including an empty one, which leaves no arguments. A spread that is not the last positional argument is rejected
  (as "too many arguments"); there is no error of its own yet.
  - `packs::IsUnboundPack(sym, scope)` tells them apart exactly, never from the type (an element could itself be
    a tuple). A comp pack is `GnCompParam` vs `GnCompArg`. A function pack is unbound unless the scope
    declares `packs::PackTypeParamName(name)` ("VariadicPackOf<name>", spelled only there).
- **Reading.** `a.N` on an unbound pack is its element type, with no bounds check (it's checked per instance, E11).
  See `UnboundPack` in `postfix_expression_operator_runtime_member_access_ast.cpp`.
- **Emission.** A function with an unbound pack is "pure generic" (`_IsPureGn`), so the template is never emitted.
  Only one instance per argument count is. FFI functions are excluded.
- **Shared helpers.** Use these; don't hand-roll:
  - `packs::IsTypePack(sym)`: an unbound `..Ts`.
  - `packs::PackElementTypes(tuple)`: a pack tuple's element types.
  - `packs::DoesArgNamePack(arg, scope)`: a lone generic argument that already is a pack (type or comp),
    which `NameGnArgsImpl` (arg_naming) forwards instead of wrapping in a tuple.

**Parser rule.** A closure's expression body must start on its parameter list's line (`_LineFeedAhead`, like a call's
`(`). Before this, `let t = ()` followed by a statement on the next line parsed as the closure `() <statement>`.

Related: [[generic-inference-engine-shape]].

**Packs bind and spread (2026-09-29).**
- `RelaxedMatch` binds a pattern's trailing pack (`Tup[First, ..Rest]`) to the tuple of the remaining arguments. An instance records its pack as one named argument (`P[Ts=Tup[..]]`), and that is taken as it is. `GenericInferenceMap` values are `Shared` so the match can own the built tuple.
- `TypeIdentifierAst::SubstituteGenerics` spreads a bound pack in a positional list (`Tup[F, R]` → `Tup[S32, Bool, U8]`). An empty pack spreads into nothing, and an unbound pack stays as its name.
- Sup attachment erases the block's pack binding, since the block stands for every element set. Binding it lost `TupLike` on every tuple (E78).
- Open (red test): a spread pattern `P[F, R]` against a non-tuple class's recorded pack.
