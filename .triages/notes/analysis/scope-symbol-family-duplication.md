---
name: scope-symbol-family-duplication
description: "Scope's symbol API is six families triplicated per symbol kind; consolidating it is deferred future work"
updated: 2026-08-28
---

`Scope` (`sources/spp/analyse/scopes/scope.cpp`) declares six near-identical families, one member per symbol kind
(Var / Type / Ns): `Add*Symbol` (+`*CheckConflict`), `Rem*Symbol`, `All*Symbols`, `Has*Symbol`, `Get*Symbol`, and
`FindInSupScopesByName*`. About 19 methods where a symbol-kind-parameterised implementation would do.

**Why deferred (decided 2026-08-28):** `FindTypeSymbol` is the profiled hot path — see [[spp-perf-hot-path]] — and
is not actually parallel to its siblings: it alone carries the `TryCachedLookup`/`RememberLookup` cache keyed on
`TypeLookupGeneration`, and the `ShiftForNamespacedType` shift for namespaced names. `FindVarSymbol` resolves
`AliasSymbol`, which the others don't. Templating the family risks pessimising the one function that matters most.

**How to apply:** revisit only with a profile in hand, and keep `Get*`/`All*` out of any first pass. The trivially
identical ones (`Rem*`, `Has*`, `FindInSupScopesByName*`) can be consolidated safely and independently. See
[[generic-leak-shared-instantiations]] for how tangled this area is in practice.

**Unused members stay (maintainer, 2026-10-16):** `AddNsSymbolCheckConflict`, `RemNsSymbol` and `HasTypeSymbol` have no
callers, but are kept so each family has one member per kind ("leave them (api symmetric)"). Don't propose removing a
family member just because it is unused.
