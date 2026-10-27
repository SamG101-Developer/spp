---
name: generic-leak-shared-instantiations
description: "Generics leak into shared concrete instantiation scopes and reappear as arguments that outrank inference"
updated: 2026-09-15
---

**Superseded 2026-09-15: the ancestor sweep and `names_an_open_generic` are gone.** With resolved types (stamps read
through `Scope::Canon`), `GetExtendedGenericSymbols` carries only the generics an instantiation's own arguments name,
and std compiles (experiment E-b, made permanent as H18). Carrying *nothing* (E-a) still breaks std: E94 in
`std/option.spp` `Opt::ior_(self, that: Opt[T])`, where `that` came out as bare `std::variant::Var`. The history
below is how it was before; a full suite on H18 was still pending when this was written.

`Scope::GetExtendedGenericSymbols` used to sweep every generic in every ancestor scope (stopping only at the first
function scope) and `RegisterGnSymbols` registers them onto the instantiation being created. A fully bound
instantiation like `Str[A=GlobalAlloc]` has **one scope shared by the whole program**, so anything swept in is
stranded there. `Scope::GetGns` then walks ancestors and offers it as a `SupGns` argument.

**Why: an argument outranks inference** (`GenericBindingSet::FromNamedArgs` seeds first; `InferGnArgs` skips names
already bound). So a callee's own `U` never gets inferred, `SubstituteGenerics` leaves it, and Stage 7 raises E26
at a meaningless position.

**How to apply:** fixed 2026-08-28 by skipping the *type* sweep when no generic argument names an open generic
(`names_an_open_generic` in `scope.cpp`). It must be decided at the capture site: by the time `GetGns` runs, a
stranded generic and a real enclosing one are both just unbound generic symbols in an ancestor scope.

**Five approaches that look right and each break std at a named site** — do not retry these:
1. Skip both sweeps when the instantiation is concrete → `w` unresolved, `atomic.spp`.
2. Drop identity (`T=T`) bindings in `GetGns` → E51, `range.spp` `Generator::send`.
3. Skip unbound type params in `GetGns` (mirroring the comp branch's own skip) → E72 `Yield`, `range.spp`.
4. Comp sweep filtered to unbound params only → `n` unresolved, `ops/iter.spp:100`.
5. `Scope::CarriedInGenerics` marker set in `RegisterGnSymbols`, skipped in `GetGns` → breaks on both
   branches; carried-in symbols are relied on as *offered arguments*, not just for name resolution.

Also fixed there: the `ignore` parameter (the comp generic whose own type is being qualified, set via
`meta->IgnoreCmpGeneric`) arrives as a `TypeAst` but was compared against symbols named by `IdentifierAst`. Those
never compare equal, so both of its filters were constants - one keeping everything, one keeping nothing - and the
inverted polarity between them cancelled out. Now matched on the name string.

So the comp sweep cannot be narrowed at all, and the asymmetry with types is real: a type parameter can sit where
no argument reaches it (`Generated[Yield or None]`), so its identity seed is load-bearing. Symptom to recognise: an
error naming a bare generic letter, reported at line 1 of an unrelated std file.
See [[spp-perf-hot-path]], [[spp-hasanalysed-conflation]].
