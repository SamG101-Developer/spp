---
name: generic-inference-engine-shape
description: "GenericSolver (generic_inference) - given layers, Unify equations, worklist solve; InferGnArgs/NameGnArgs/arg_naming are gone; plus traps hit adding comp->type (2026-09-28)"
updated: 2026-10-26
---

Since 2026-09-28 inference is `generic_inference::GenericSolver`, one per use site (a call candidate, or a generic
type name in `TypeIdentifierAst::Stage7`). `InferGnArgs`, `GenericBindingSet`, `NameGnArgs` and the `arg_naming` module
are gone.
- **Given layers** (`Give`, first binding per name wins): written args (named by the pure `NamedGnArgs`), then
  `ReceiverGns`, then the sup block's generics, then the `Self` pin. The pin is `emit`ted into the solution only
  for a concrete receiver. `Give` takes the Vec by value: moving elements out of an rvalue ref left the caller's
  `GnArgGroup->Args` full of nulls, which crashed `MatchArgListArity` mid-solve.
- **Equations** (`Unify`): a call's parameter slots (`BindSlots` in overload_resolution); an object initializer's
  attributes via `TypeIdentifierAst::InferFromAttributes` on the type's last part. `meta.InferSource/InferTarget` no
  longer exist.
- **Solve**: equations (given types are skipped), then a worklist over comp→type and constraints (each entry read once,
  once bound), then defaults, conflicts, uninferred, cross-substitution, `_CheckCompArgTypes`.
- A comp value offered by a match points into the matched type, so the solver keeps matched source types alive
  (`_OwnedTypes`). Re-reading a comp value while `U32`'s own alias is resolving (stage 4) crashed, which is why
  entries are read once.

**Traps (2026-09-28):**
- **A literal's `InferType` returns its symbol's cached `FqName()` node.** It is shared by every use of that type.
  Binding it without `AstCloneShared` let a stage-4 analysis poison `U32`'s resolution, and std lost `lt` ("i <
  limbs", E26). Same family as the shared-FqName trap in [[spp-method-as-value-gap]].
- **Comp values that aren't literals** are only read once sups are attached (`kAttachSupScopes`). Literals are read from
  `kResolveDeclarations`, so signature and attribute types infer too.
- **Instantiating a function before the template's own stage 7** (a comp expression `n + 1_uz` in a signature is checked
  in stage 6) cloned `self: Self` unresolved, which led to `uadd[Self]` and E26 in intrinsics.
  `CreateGnFnScope` now resolves `Self` once, from the instance root, and substitutes it through the subtree via
  `SubstituteVarSymTypes(..., self_type)`. Calling `FindEnclosingSelfType` per subtree scope segfaulted.

**Variadic comp packs:** `VariableSymbol::IsVariadic` mirrors `TypeSymbol::IsVariadic`. It is set in
`GenericParameterAst`, copied onto bindings in `RegisterGnSymbols`, and read by `IsCompPack` so a lone `A[n]` forwards
the pack. A comp parameter's placeholder `cmp` global is typed from the symbol, not from `CompType`, because a pack
holds a tuple.

**Test-writing:** unsuffixed literals are `S32` and never adapt (`n + 1` for a `USize` n is E51). Destructuring a
generic class with no arguments (`let A() = x`) is a separate gap, so use `drop`.

**Defaults (2026-09-29).**
- A call default feeds inference only when its type is fully concrete. After solving, it is remade from the instantiation's own parameter, with the whole solution applied.
- A comp default is translated from `GenericParameterAst::WrittenCompDefault`, because the parameter's own `CompDefault` is analysed and so desugared (`n + 1_uz` becomes `n.add(1_uz)`). It is folded when closed, and `_CrossSubstitute` re-derives it once all its names are bound.
- `GenericParameterAst::Stage7` folds a closed comp default to prove it constant, and skips one that names a comp param
  (`IsClosedCompId`). This was `GenericParameterGroupAst::Stage9` before the Stage 9 walk went (N7.11, 2026-10-26).
