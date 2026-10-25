---
name: analyse-utils-module-map
description: "Where analyse::utils functions live: the 2026-09-28 reorg (old module → new module) and the 2026-10-16 regrouping (fn_signatures, control_flow folded, twins moved)"
---

On 2026-09-28, Phase 1 of `.triages/docs_closed/analyse-utils-reorg.md` moved and renamed modules with no logic change. The doc's
target-layout table is the map. Renames: type_utils→type_resolution, generic_bindings→generic_inference,
monomorphization_utils→monomorphization, overload_utils→overload_resolution, func_utils→fn_values,
cmp_utils→comp_time_intrinsics, mem_info_utils→memory_state, bin_utils→operator_desugaring.

**Out of analyse::utils:** resolution_index → `spp::lsp`; builtins → `spp::codegen`.

**New modules:** packs, marker_sups, self_type, member_lookup, control_flow, comp_generics, regions, borrows. (arg_naming was added then deleted the same day: generic naming is `generic_inference::NamedGnArgs`, fn naming is `overload_resolution`'s `BindSlots`.)

**Deleted modules:**
- instantiation_queue went into monomorphization, as `EnqueueInstantiation`/`PopInstantiation`/`ClearInstantiations`.
- assignment_utils went into regions (IsIdentifier/IsAttr/IsDeref).

**Promoted from anon to exported:** `monomorphization::PotentiallyGenerateGnSubstitutedPrototype`,
`regions::DescendToPart`/`RegionConsumed` and `drop_utils::CheckDestructorStillReachable`, because their callers
now live in other modules.

**Phase 2 shared helpers (same day), to use rather than hand-roll:**
- `type_members::SuperClassTypes` / `SuperClsNames`: a type's class sups.
- `memory_state::ScopeSnapshot` + `SnapshotSymbols` / `SnapshotScopes` / `RestoreSnapshot`.
- `memory_state::MarkInconsistentPaths`: merges branch and loop-exit states.
- `mem_utils::RaiseIfMoved` (`RaiseIfInconsistent` - initialized, moved, partially moved, escaping borrow - is file-local in mem_utils.cpp since 2026-10-02).
- `comp_generics::ResolveCompArg`: fold, else the bound value.
- `Symbol::ParamIdentity()`.
- `DedupVariableInnerTypes` is now `type_compare::VariantMemberTypes`.
- `MemoryInfo::MovedBy` clears partial moves itself.

**2026-10-16 regrouping** (functions moved to the module of their twin or their job):
- `ValidateInconsistentMemory`: memory_state → case_utils (beside `JoinBranches`, which replaced `ValidateInconsistentTypes` in N9.1).
- `CheckForConflictingOverload`, `SameSignature`, `CheckForConflictingOverride`: fn_values → new `fn_signatures`.
  fn_values no longer imports overload_resolution.
- `VariantMembers`, `VariantMemberRefs`, `OrderVariantMembers`: type_compare → type_members.
- `ConvertIsExprToFnCall` → `operator_desugaring::ConvertIsExprToCase`.
- `DoesTypeNameAGnParam` (from type_resolution) and `IsOrExtendsTemplate` (from type_members) → type_predicates.
- `RegisterSelf`, `RegisterProvisionalSelf`: sup_blocks → self_type.
- `IsDestructurePlaceExpression`: regions → destructure_utils. `regions::IsIdentifier` inlined.
- control_flow deleted; `Diverges` is in expr_utils.
- Renames: `IsTypeRecursive` → `FindRecursiveAttrType`; `IsIndexWithinBound` → `IndexBoundOf` (returns the bound).

**Why:** memory files written before this date may name the old module. Most qualified names were rewritten, but
prose may still say e.g. "func_utils".

**How to apply:** check this map before grepping for a helper under an old module name. When scripting import fixes, only edit the failing TU:
adding imports to `utils/ptr.ixx` or `asts/utils/ast_utils.ixx` broke gcm loading ("Bad file data", "failed to load
pendings").

Related: [[variadic-pack-model]], [[diverges-single-definition]].
