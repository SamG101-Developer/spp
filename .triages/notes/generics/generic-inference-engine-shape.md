---
name: generic-inference-engine-shape
description: "GenericSolver: two constructors, given layers, equations and evidence, one matcher, worklist solve; the shared helpers new inference sites hook into (I5, 2026-11-19)"
updated: 2026-11-19
---

Inference is `generic_inference::GenericSolver` (since 2026-09-28; `InferGnArgs`, `GenericBindingSet`, `NameGnArgs` and
`arg_naming` are gone), one per use site. Its edges were restructured in I5
(`.triages/docs_open/generic-inference-sites.md`), so a new inference site plugs into the pieces below instead of adding
its own.

**Constructing one.**
- A call candidate: `GenericSolver(params, owner_scope, sm, meta)`, in `overload_resolution::CheckCandidate`.
- A type name: `GenericSolver(type_sym, sm, meta)`, which also reads a `use` or `type` alias's declared types through
  its target (`ReadDeclaredWith`), so they name the alias's own parameters. Used by `TypeIdentifierAst::_SolveArgs`
  and `_RecheckArgs`.

**Given layers** (`Give`, first binding per parameter wins, so the order is the precedence):
1. the written arguments, named by the pure `NamedGnArgs`;
2. the receiver's, matching it against the method's block pattern (`ReceiverGns`);
3. what the block itself binds (`BoundGnsOf(ReachedThrough)`);
4. the `Self` pin (`PinSelfToReceiver`), emitted into the solution only for a concrete receiver.

`Give` takes the Vec by value: moving out of an rvalue ref once left the caller's `GnArgGroup->Args` full of nulls.

**Equations** (`Unify`, a source type given where a target type is declared):
- a call's parameter slots (`BindSlots`, then `InferAllGns`);
- for a type name, the `Evidence` a use site hands it (`TypeIdentifierAst::InferFrom`, applied by `GiveEvidence`):
  - `Equations`: an object initializer's attributes, own and inherited (`type_members::GetAllAttrs`);
  - `Subject`: a destructure's value. Its candidates are matched against the name's pattern, then picked
    (one, else E129, else none); see [[destructure-infers-from-subject]].

  A new source of evidence is a new `Evidence` field, not a member and hook on the AST.
- a static call's bare receiver is inferred before resolution, from each block's method solved against the arguments
  (`InferTemplateReceiver`; [[static-call-receiver-inference]]).

**Matching** is one path: `_Match` → `_MatchIds` → `scopes::UnifyTypeIds`. For a target with a shape to match, it
falls back to the given type's super classes (`type_members::UnifyInHierarchy`), then, for two borrows of the same
convention, to what the given borrow forwards to (`marker_sups::BorrowForwardTarget`: `&Arr[S32, 2]` binds the `T` of
a `&View[T]`). Forwarding comes last, so it never displaces a match, as in the argument check
([[overload-arg-matching-order]]).

**Solve:**
1. the equations (written parameters are skipped);
2. a worklist over comp values and constraints (each entry read once, once bound; `_ReadConstraints` matches through
   `UnifyInHierarchy` too);
3. defaults, conflicts, uninferred;
4. cross-substitution;
5. `_CheckCompArgs` and `_CheckTypeArgs` (whose constraint core is `type_compare::UnmetConstraint`).

Re-reading a comp value while `U32`'s own alias is resolving (stage 4) crashed, which is why entries are read once.

**Results** stay on the solver (a separate `Solution` type was dropped, I5.6):
- `SolvedArgsId()`: the instance's identity;
- `TakeArgs()`: the arguments written back into a name;
- `InferredParams()`: bound, but neither given nor defaulted.

`KnownBindings()` is read *before* solving: defaults, and filtering a subject's candidates.

**Shared helpers to reach for, not re-write:**
- `type_members::UnifyInHierarchy`: a type, else its first matching super class.
- `scopes::PatternsOverlap`: either pattern takes the other.
- `TypeSymbol::GnSelfId()`: a class as itself over its own parameters, by identity.
- `type_resolution::AnalyseInferredType`: substitute `Self`, attach `Evidence`, analyse. The substitution is a copy,
  so evidence attached before it is lost.

**Traps (2026-09-28):**
- **A literal's `InferType` returns its symbol's cached `FqName()` node.** It is shared by every use of that type.
  Binding it without `AstCloneShared` let a stage-4 analysis poison `U32`'s resolution, and std lost `lt` ("i <
  limbs", E26). Same family as the shared-FqName trap in [[spp-method-as-value-gap]].
- **Comp values that aren't literals** are only read once sups are attached (`kAttachSupScopes`). Literals are read from
  `kResolveDeclarations`, so signature and attribute types infer too.
- **Instantiating a function before the template's own stage 7** (a comp expression `n + 1_uz` in a signature is checked
  in stage 6) cloned `self: Self` unresolved, which led to `uadd[Self]` and E26 in intrinsics.
  `CreateGnFnScope` now resolves `Self` once, from the instance root, and substitutes it through the subtree
  (`ReadVarSymbolTypes(..., self_type)`). Calling `FindEnclosingSelfType` per subtree scope segfaulted.

**Variadic comp packs:** `VariableSymbol::IsVariadic` mirrors `TypeSymbol::IsVariadic`. It is set in
`GenericParameterAst`, copied onto bindings in `RegisterGnTypeSymbol`/`RegisterGnCompSymbol`, and read by `IsCompPack`
so a lone `A[n]` forwards the pack. A comp parameter's placeholder `cmp` global is typed from the symbol, not from
`CompType`, because a pack holds a tuple.

**Test-writing:** unsuffixed literals are `S32` and never adapt (`n + 1` for a `USize` n is E51). Destructuring a
generic class with no arguments (`let A(..) = x`) infers from the value since 2026-11-19
([[destructure-infers-from-subject]]).

**Defaults (2026-09-29).**
- A call default feeds inference only when its type is fully concrete. After solving, it is remade from the
  instantiation's own parameter, with the whole solution applied.
- A comp default is translated from `GenericParameterAst::WrittenCompDefault`, because the parameter's own
  `CompDefault` is analysed and so desugared (`n + 1_uz` becomes `n.add(1_uz)`). It is folded when closed, and
  `_CrossSubstitute` re-derives it once all its names are bound.
- `GenericParameterAst::Stage7` folds a closed comp default to prove it constant, and skips one that names a comp param
  (`IsClosedCompId`). This was `GenericParameterGroupAst::Stage9` before the Stage 9 walk went (N7.11, 2026-10-26).
