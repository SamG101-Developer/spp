---
name: spp-hasanalysed-conflation
description: "_Resolved exists but is NOT carried by Clone; doing so perturbs diagnostics, because the visibility check is order-dependent"
updated: 2026-09-04
---

`TypeIdentifierAst::_HasAnalysed` means the stage checks have run (cleared by `ResetCache()`, which eight prototype
sites use to force stage-7 analysis early for error ordering). `_Resolved` means the node's **value** has settled -
generic args named and qualified, `Self` resolved, variants collapsed - and is never cleared. Only `ToView()` keys off
it today.

## The settle/check split was built and then backed out (2026-09-04)

Gating the settling half of `Stage7_AnalyseSemantics` on `not _Resolved`, and carrying `_Resolved` through `Clone()`,
works and is worth **~15% wall clock** (settle skipped on 88k of 260k eligible analyses, up from 12k without the clone
propagation). It was still reverted, on the maintainer's call - *"this is too messy, this hooks into too much"*.

**Why it hooks into so much:** the module-visibility check is *order-dependent*, and any change to how much analysis
is skipped moves which diagnostics come out. Measured on pristine HEAD with `std::net::udp::UdpSocket` (no annotation,
so private to its module), a private type is reported in a `let` type, an alias target, an object initializer and a
static member access - and **silently allowed** in a function parameter, a return type, a class attribute, a generic
argument and a `sup ... ext` superclass. Whether it fires comes down to which copy of a prototype a later pass happens
to hold.

Root cause: a type symbol's visibility is only synced from its annotations in `ClassPrototypeAst::Stage5_LoadSupScopes`,
so nothing before `kPreAnalyseSemantics` has an answer - while `FunctionParameterAst::Stage7_AnalyseSemantics`
(2387 calls at `kLoadSupScopes` vs 82 at stage 7) and `ClassAttributeAst::Stage5_LoadSupScopes` both replace the
written occurrence with `FqName()` *during* stage 5. The check only ever fires because
`FunctionPrototypeAst::Stage6_PreAnalyseSemantics` re-analyses `param->Type` and sometimes still finds a written node.

**Fixing the check properly is the prerequisite, and it is its own piece of work.** A full fix was built and reverted:
`DecideVisibility` as the single implementation, a deferred `{symbol, occurrence, accessing scope}` list drained at the
start of stage 6, `_VisibilityChecked`/`_VisibilityDeferred` carried by `Clone()`, and a recursive walk of
`Source.OriginalOldType` for alias targets. It made all ten type positions report correctly with no false positives -
but it touches `visibility_utils`, `compiler_boot`, `type_statement_ast` and the `TypeIdentifierAst` flag set.

**Four dead ends, do not retry:**
- Marking the qualified form `MarkSourceWritten()` reports violations against unrelated signatures: `FqName()` names
  the class an alias resolves to (public `F32` → package-private `SizedFloatingPoint`), and the
  `type_sym->Name->Name == Name` guard is what keeps the check on the symbol the site named. Cloning first to detach
  from the shared `FqName()` node does not help.
- Re-*resolving* the name at drain time rather than recording the symbol: scopes are cloned and added to between
  stages 5 and 6, so the same name in the same recorded scope comes back as a different symbol (`Bool`/`Void`
  reported private inside std).
- Deferring without honouring `meta.IgnoreAccessModifierViolations` - the flag is only read inside the check, so an
  exempted access got recorded then policed later.
- Any process-wide list needs resetting per compile; `spp_tests` runs many compiles in one process and every
  `SHOULD_FAIL` test throws before the drain. See [[spp-test-harness-stale-std]].

**Still open, verified pre-existing:** a private module-level *function* (`std::intrinsics::add`) is not reported at
the call site - a different path (`identifier_ast.cpp` / static member access).

Related: [[spp-perf-hot-path]], [[ast-clone-carries-cached-analysis]], [[spp-ir-diff-verification]].
