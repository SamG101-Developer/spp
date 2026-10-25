---
name: spp-perf-hot-path
description: "spp compiler time has moved from type analysis to monomorphization; what's already optimised"
updated: 2026-10-16
---

Updated 2026-08-22 after a round of optimisation. Profile with a debug-build flamegraph over ./project;
see `no-benchmark-runs` for why not to time it, and [[spp-ir-diff-verification]] to check transparency.

**Already done** (don't redo): `TypeSymbol::FqName` cached; `NsParts`/`TypeParts` return borrowed
pointers; `IdentifierAst` equality + hashing use the interned id (`spp-interned-symbol-keys`);
`GetUnimplementedAbstractMethods` memoised per scope; `ToView` fast path for generic-free types;
`CompilerMetaData::Save`/`Restore` swap+clear instead of move-assign.

**Two generation counters, deliberately separate** (`scope.ixx`): `ScopeLinkageGeneration` (parent
re-parenting only) guards the FqName cache; `TypeStructureGeneration` (parent *and* sup-scope changes)
guards the abstract-methods memo. Folding them into one doubled FqName's cost, because sup scopes are
attached constantly during monomorphization and a qualified name doesn't depend on them.

**Where time is now**: monomorphization, not type analysis. `MakeSupInstance` ~25%,
`FindOrMakeClsInstance` ~11%, AST `Clone` ~12%, `SubstituteGenerics` ~5%. Scope/symbol-table *cloning*
was never the issue — that was checked and is <1%.

Resolved: `MakeSupInstance` was rebuilding instantiations the super-scope pass had already made
(2,463 of 11,871 calls), leaving orphaned scopes in the tree. Now memoised on
(template block, owning type scope, substituted name) - worth 5% of the run. The remaining 9,408
instantiations are legitimate; cutting further needs fewer instantiations, not cheaper ones.

Also done: `NsParts`/`TypeParts` gained appending `*Into` forms, so a type chain is one allocation
rather than one per level (`ShiftForNamespacedType` 15.5% -> 8.0%, worth 7.5% of the run).

**2026-09-29 (TypeId migration regressions, profiled with `perf record --call-graph dwarf` + a perf-script aggregator):**
- **Nothing keyed on `TypeStructureGeneration` hits during stages 5.5 and 7.** Every instantiation bumps it. Validate a cached answer against the inputs instead: the abstract-methods memo now stores its `GetSupScopes` list and revalidates by comparing it.
- **`OnSupScopesRead` must return before `MetaGuard`** when the scope is already attached. `GetSupScopes()` now uses its cache during 5.5 as well.
- **`FindInSupScopesByParam` re-walked diamonds without a seen set.** It now uses `GetSupScopes()`.
- **`CheckConflictingTypeOrCmpStatements`** returns early for a block with no `type`/`cmp`.
- **`HeadOf`/`ParamsOf`/`BareTypeId` are cached on the interned `InstanceKey`**, via mutable pointer fields. A map memo was still about 6% self time.
- Result: stage 5.5 went from 34% to 25% of a `./project` build, and stage 7's abstract check from 23% to about 5%. HEAD is not a buildable baseline: the index is partially staged.

**2026-10-16 release profile (`.tools/generate-flamegraph-release.sh` steps by hand; `spp test --all-libs` in
`project/`, `perf script --comms spp`, folded stacks aggregated):** ~55% of the 611-test run is LLVM optimising ONE
function, the generated test runner `main` (`CompilerBoot` writes one `case` per test; 43k insts / 5.9k blocks after
LTO; jump threading + dominator-tree updates). With `-f regression` (108 tests, 4k-inst main) LLVM drops from 7.2 s
to 0.5 s. LLVM has no frame pointers, so its stacks don't reach our frames: compare by filtering the workload.
Frontend (108-test run): stage 7 23% (overload resolution 16%, of which every candidate's instance 5.5%), stage 5.5
sup attach 16% (`MakeSupInstance` 11%), drain 11% (body analysis 9%); allocation ~10% plus AST clone/destroy ~6%;
`InternTypeKey` 3.4% self (from `Scope::TypeIdOf`); symbol-table `Find` 2.9% (most from `FindNsSymbol`).

Next candidates: shared_ptr traffic is now the largest cross-cutting cost (~23%), much of it the
`WithoutGns` cluster (~5.4%, gated behind a "using the cache breaks" Todo in
type_unary_expression_ast.cpp that needs root-causing); AST `Clone` in monomorphization (~13%);
`dynamic_cast` (~4.3%, the `To<T>()` helper, would need an LLVM-style kind tag).
