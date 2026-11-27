---
name: destructure-infers-from-subject
description: "An object pattern's unwritten generic args come from the value it takes apart; a variant offers its members, written args filter them, more than one left is E129"
updated: 2026-11-19
---

`let Box(v) = b`, `case o is Some(val)` and nested patterns (`Some(val=Some(val))`, `(Some(val as a), ..)`) take
the generic arguments they do not write from their subject's type. The pattern's type name gets the subject as
evidence (`TypeIdentifierAst::InferFrom({.Subject = ..})`, the hook the object initializer gives its attribute
equations through), set by `type_resolution::AnalyseInferredType`. That one helper is used by the object initializer,
the `let` destructure, the `case` object pattern and `AnalyseNestedWrittenTypes`, which passes each nested pattern its
own subject: the attribute it binds, or the tuple or array element.

In the solve (`generic_inference::GiveEvidence`), the candidates are the subject itself, or each variant member, that
is this class:
- **Written arguments come first.** They are given first, and a candidate disagreeing with any of them
  (`KnownBindings`) is dropped, so `is A[S32](v)` picks one member of `A[S32] or A[Bool]`.
- **One candidate left:** its bindings are given below the written ones. A written argument that disagrees with a
  non-variant subject surfaces as the destructure's own E1.
- **More than one left:** E129 `SppDestructureGenericAmbiguousError`.
- **None left:** nothing is given, so the old E72 or E1 stands.
- **Every candidate is matched against the name's pattern** with `UnifyTypeIds`: a class's `GnSelfId()`, or for a
  `use` or `type` alias its `AliasTargetId()`. An alias declares parameters of its own, so the class's
  `InstanceBindings` would be keyed by the wrong ones. The prelude's `Some`/`Pass`/`Fail` are `use`s, which is why
  only locally declared classes inferred before this was handled.

**Traps:**
- `AnalyseWrittenType` substitutes `Self` into a copy, so a hook set before it is lost. Substitute first, then set
  the hook, then run `Stage7`, as the object initializer does.
- Pass a resolved type to an error through `ErrTypeAt`, never `*scope->TypeAstOf(id)`: see [[err-args-hold-references]]
  (that temporary segfaulted E129).

**Why:** std spelled about 120 patterns' generics by hand (`is Some[T](`), and SPP-STL's `Arr::split_at` destructures
`ArrParts[T, k, n - k]`.

**How to apply:** a new pattern kind that names a type takes its subject through `AnalyseInferredType`. Tests:
`LocalVariableDestructureObjectGeneric.*`, `CasePatternVariantDestructureObjectGeneric.*`. See
[[spp-generic-inference-is-one-way]].
