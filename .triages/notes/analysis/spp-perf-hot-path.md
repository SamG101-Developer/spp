---
name: spp-perf-hot-path
description: "spp compiler time has moved from type analysis to monomorphization; what's already optimised"
updated: 2026-11-25
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
- **`HeaderOf`/`ParamsOf`/`BareTypeId` are cached on the interned `InstanceKey`**, via mutable pointer fields. A map memo was still about 6% self time.
- Result: stage 5.5 went from 34% to 25% of a `./project` build, and stage 7's abstract check from 23% to about 5%. HEAD is not a buildable baseline: the index is partially staged.

**2026-10-16 release profile (`.tools/generate-flamegraph-release.sh` steps by hand; `spp test --all-libs` in
`project/`, `perf script --comms spp`, folded stacks aggregated):** ~55% of the 611-test run is LLVM optimising ONE
function, the generated test runner `main` (`CompilerBoot` writes one `case` per test; 43k insts / 5.9k blocks after
LTO; jump threading + dominator-tree updates). With `-f regression` (108 tests, 4k-inst main) LLVM drops from 7.2 s
to 0.5 s. LLVM has no frame pointers, so its stacks don't reach our frames: compare by filtering the workload.
Frontend (108-test run): stage 7 23% (overload resolution 16%, of which every candidate's instance 5.5%), stage 5.5
sup attach 16% (`MakeSupInstance` 11%), drain 11% (body analysis 9%); allocation ~10% plus AST clone/destroy ~6%;
`InternTypeKey` 3.4% self (from `Scope::TypeIdOf`); symbol-table `Find` 2.9% (most from `FindNsSymbol`).

**2026-11-01 sup attachment (`./project`, about 3 s, `build -m dev`):** 27% of the run, of which `MakeSupInstance` 17% from 27,289 instances at about 20 us each: count, not cost per call. The memo above is gone from `MakeSupInstance`, and what replaces it is `Scope::SupsAttached`. Read through the door, 72% of instances belong to types analysis reads, 22% only to the attach machinery, 6% to nothing, so attaching whole types lazily is worth at most about 5%. Two things that do nothing, measured over 40 pairs: keying a mock's own block lazily (0.6% in the profile, not timeable), and skipping `ReadVarSymbolTypes` and the super class for instances their constraints reject (1,812 of them, all cheap). Time such changes in alternating pairs: differences of a few percent vanish between runs otherwise. NumEx was 23% of the run until 2026-11-01: [[sibling-libs-install-release]].

**2026-11-02 one gtest process (`ClosureExpressionAst.test_valid_closure_correct_return_type`, release, 1.8 s idle, 840 MB peak, one `perf --call-graph dwarf` sample):** every test compiles all of std. Stage 7 28.5%, 5.5 24.1%, 9.5 6.6%, 6 6.6%, 11 6.4% (LTO link 3.4%), 5 5.1%, 10 4.2%, lex + parse 4.8%, `~Compiler` 7.7%, allocator 14% across all of them. `APInt` arithmetic was about 4%, from `WriteFloat` under `ValueKeyOf` ([[apfloat-tostring-not-rounded]]). The gtest binary now skips `~Compiler` unless another compile follows or the build defines `SPP_TEARDOWN` (Debug, sanitizers, coverage). With both, 14 alternating pairs gave -6.8% on a passing test and -11.8% on a rejected one; `~Compiler`'s 7.7% from the dwarf profile overstated it a little, and static destructors at exit take only about 3 ms. Deleting every std module that test can lose removes 15% of the lines but saves only 12% (1.8 to 1.59 s), so pruning std is no route to a cheap test. CI's sibling libs are Release (`cmake_install` passes it; `.cold` clones in the binary).

Next candidates: shared_ptr traffic is now the largest cross-cutting cost (~23%), much of it the
`WithoutGns` cluster (~5.4%, gated behind a "using the cache breaks" Todo in
type_unary_expression_ast.cpp that needs root-causing); AST `Clone` in monomorphization (~13%);
`dynamic_cast` (~4.3%, the `To<T>()` helper, would need an LLVM-style kind tag).

**Per-written-type checks are hot (2026-11-04).** Anything `TypeIdentifierAst::Stage7` does for every written type
shows in the compile counters (`benchmarks/run.py`, deterministic, so a before/after of `hello` and `generic-depth` at
one round settles it). Three such costs found after the pack redesign, each fixed without changing behaviour:
- a check doing its own `FindTypeSymbol`: reuse the symbol the main path found (E124's check, -34k lookups). Keep
  what it found on the node (`_TypePack`) for the early-return paths: a signature's types are first resolved while
  sups are attached, before most checks apply, and every later analysis returns early, so a main-path-only check
  never fires;
- a sup walk asked per type (`marker_sups::HoldsNoValueOfItsArgs`, P0's exemption): cached on the `TypeSymbol`
  (`HoldsNoValueOfArgs`) once sups are attached (-63k type-key interns);
- `type_compare::UnmetConstraint`'s `Self` fallback substituted every sup eagerly: only on a miss, and only sups of the
  constraint's own template (`HeaderOf(...).Ptr`), -643 distinct keys.
To find a new increase: per-phase counter diffs first, then switch one suspect off per run.

**2026-11-25 release profile (`./project`, `build -m dev --skip-vcs`, 4.5 s, peak RSS 1.55 GB):** about 31% of samples
have no spp frame, because mimalloc and LLVM lack frame pointers; most of them (15.5% of the run) are the kernel
zeroing pages that mimalloc touches first (`mi_page_free_list_extend` > page fault > `clear_page_erms`), so footprint
is the largest single cost. Under `Compile`: Stage 7 17%, Stage 5.5 14% (`MakeSupInstance` 13.7%, all from
`AttachSpecificSuperScopes`), Stage 9.5 6.6%, Stage 6 6.1%. Type-AST clones are about 6% inclusive, mostly
`TypeAst::WithSourceSpanAt` (always clones) and `TypeRef::AstIn` (clones `FqName`), chained at
`postfix_expression_operator_function_call_ast.cpp` (`AstIn(...)->WithSourceSpanAt(...)` clones twice).
`Compiler::~Compiler` at the end of `HandleBuild` is 5.5%. Done since: `cli.cpp`'s `StartCompile`/`HeldCompile` park every compile (build, test, gtest) and free it
only before the next one, or never (`SPP_TEARDOWN` builds free it with its holder); the rvalue `WithSourceSpanOf`/`At`
stamp a uniquely held root in place (`ReadTypeOrNull`, `TypeInferrableAst::InferType`, `GenericSolver::_AstOf`). Re-profiled
after both: user time under `HandleBuild` 3.40G to 2.91G samples (-15%; `~Compiler` gone, `WithSourceSpanOf`/`At`
-72%/-77%, type-AST clones -26 to -33%, `~TypeUnaryExpressionAst` -59%). That run recorded no kernel frames at all
(the page faults landed on `mi_page_free_list_extend` instead), so compare totals only between graphs recorded the
same way: count kernel frames (`asm_exc_page_fault`) in both first.

**2026-11-25 heaptrack (`./project`, one build, `cmake-build-release-heap` per [[heaptrack-needs-mimalloc-removed]]):**
41.4M allocation calls, 88% of them 256 B or less, only 4.55M (11%) temporary; peak heap 1.35 GB, of which 1.30 GB is
still held at exit, so the heap only grows. It is mostly small objects kept, not churn. Type-AST clones are about
11M calls and 360 MB of the peak (`TypeIdentifierAst::Clone` 140 MB, `TypeUnaryExpressionAst::Clone` 102 MB,
`TokenAst::Clone` 44 MB, `GenericArgumentGroupAst::Clone` 37 MB, plus a `shared_ptr` control block per
`GenericArgumentAst::Clone`, 3.3M calls); `Scope::Scope` 180 MB over 270k scopes. Pure churn, nothing kept:
`GenericParameterGroupAst::GetAllParams` 1.3M, `ArgsOf` 1.05M, `GenericArgumentGroupAst::GetAllArgs` 0.8M,
`Scope::FindNsScope`'s vector 0.75M. `heaptrack_print -F` writes 19 GB per cost type here: use its `-a 1 -p 1 -n N`
tables instead. After `CloneShared`, `ArgsView`, the `Span` arguments and
`FindNsScope` by recursion: 36.1M calls (-12.8%), peak unchanged, IR identical. Left (calls per build): the
the two `Stage1_PreProcess` member snapshots (module 0.33M, sup), which must stay: function pre-processing inserts
into and removes from the list being walked; `AstBody`'s `Vec<Ast*>` (five member types, no one `Span`); `TypePartsInto`/`NsPartsInto`
0.6M; `SubstId` copying and sorting both lists 0.49M; `BindingsOfArgs` 0.41M; `order_utils::DoOrderArgs` 0.42M;
`IndividualSymbolTable::GetAll` 0.23M; the function-group `GetAllParams`/`GetNonSelfParams`/`GetAllArgs`.

**2026-11-25 A/B, `CloneShared` (`make_shared`) vs two allocations, 10 alternating pairs over `./project`:** median
3.51 s vs 3.48 s (A faster in 6 of 10, by 0.055 s: a tie), peak RSS 1491 vs 1516 MB. Kept for the memory. The
flamegraph's +18% "clone work" was the page-fault cost moving onto the frame that first writes the object. Trap: a
default `Ast::CloneShared` returning `Shared<Ast>(Clone())` leaves the `enable_shared_from_this` base of
`IdentifierAst`/`TypeAst` (below `Ast`) unset, so `shared_from_this` on the clone throws `bad_weak_ptr`; the default
returns null and `AstCloneShared` wraps `Clone` as the type asked for (`test_msc_ast_clone.cpp`).
