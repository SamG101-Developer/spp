---
name: scope-defined-in-can-dangle
description: "TypeSymbol::ScopeDefinedIn is the use site and can be a freed scope; never resolve through it after Stage 7"
updated: 2026-09-13
---

An instantiated class symbol gets `ScopeDefinedIn = sm->CurrentScope` (monomorphization `FindOrMakeClsInstance`), and `CreateGenericSym` does the same for generic-arg symbols. That use site can be a body scope of a generic function instantiation, which `AnalysePendingGnSubstitutions` frees in Stage 9.5 (`ProtoScope()->Children.Clear()`). The symbol outlives it.

This was the intermittent `spp test --all-libs` segfault in `IndividualSymbolTable<.., NamespaceSymbol>::Get` (2026-09-13): `IsThreadSafeRec` resolved `Tup`/`Var`/`Arr`/... generic args through `ScopeDefinedIn`. It crashed only when the freed block was reused, so only under some layouts: never under gdb, 2 of ~16 plain runs. Fixed by resolving through `LinkedScope`.

**Why:** freed memory usually still reads fine, so the bug hides; the gtest suite stops before monomorphisation and can't see it.

**How to apply:** resolve names through `LinkedScope` (lives as long as the symbol) or a module scope, not `ScopeDefinedIn`, in anything that can run in Stage 9.5+. `HasThreadSafeConstraint` still uses `ScopeDefinedIn` (unbound generics only) and is a known remaining risk. For a crash like this, go straight to the ASAN build; see [[asan-build-for-llvm-crashes]].
