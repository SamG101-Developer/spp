---
name: identity-renames-2026-10-06
description: "Old -> new names from the 2026-10-06 identity/lookup rename (InstanceKey->TypeKey, CompNode->CompKey, Written*->*ById/Bound, Record*->Stamp*, ...); older memories and docs use the old names"
updated: 2026-10-06
---

Applied by the maintainer's request 2026-10-06 (backup: scratchpad `repo-backup-pre-rename-2026-10-06.tar.gz`, HEAD f67f1b39).
Older memories and docs (analyse-cleanup-review.md, generic-architecture.md, ...) still use the left column.

- Structs: `InstanceKey` -> `TypeKey`, `InstanceKeyHash` -> `TypeKeyHash`, `CompNode` -> `CompKey`, `CompNodeHash` ->
  `CompKeyHash`; module/files `spp.analyse.scopes.instance_key` (`instance_key.ixx` / `.cpp`) -> `type_key`
  (`comp_key` already matched).
- Lookups by id: `Scope::TypeSymbolOf` -> `FindTypeSymbolById` (raw: a param is itself),
  `FindWrittenTypeSymbol` -> `FindBoundTypeSymbolById` (what it means here: binding leaf, read-in instance, alias
  copy); comp twins `VarSymbolOf` -> `FindVarSymbolById`, `FindWrittenVarSymbol` -> `FindBoundVarSymbolById`;
  `GnTypeParamOf` / `GnCompParamOf` -> `FindGnTypeParamById` / `FindGnCompParamById`.
- `WrittenTypeIdOf(TypeSymbol)` and `WrittenCompIdOf(VariableSymbol)` -> `NameTypeIdOf` (overloads; the stamp a name
  of the symbol carries, scope-free).
- AST stamps (same day, second pass): `SetWrittenTypeId` / `WrittenTypeId` -> `StampTypeId` / `StampedTypeId`,
  `SetWrittenCompId` / `WrittenCompId` -> `StampCompId` / `StampedCompId`, `SetWrittenTemplateId` -> `StampTemplateId`,
  fields `_Written*Id` -> `_Stamped*Id`. "Written" now only means source as written (`AnalyseWrittenType`,
  `MarkSourceWritten`, `WrittenText`).
- `BareTypeId` -> `TypeIdWithoutConvention` -> (2026-10-07) `BareOf`; `ParamsOf(TypeId)` -> `ParamsNamedBy`, `CompKeyParams` ->
  `ParamsNamedBy(CompKey)` -> (2026-10-07) `ParamsNamedBy(CompId)` (and `MembersNamedBy(CompId)`), `CompKeyMembers` -> `MembersNamedBy`; `ParamsOfGroup` -> `ParamsDeclaredBy`;
  `type_resolution::ParamsOfArgs` -> `ParamsBoundByArgs`.
- `TypeKey::PushId` -> `PushTypeId`; (2026-10-07) `InternWord` / `WordText` -> `TextWord` / `TextOfWord`; `TypeKey::AppendKey` -> `PushSpliced` (public), `Append`/`AppendWords` -> private `_Append`/`_AppendWords` (+ private `_AppendKey`, `_DecodePart`; `HeadOf`/`BareOf`/`ArgsOf` are friends); free `PushTypePart(key, part)` / `PushCompPart(key, id)` -> members `key.PushTypePart(TypeId)` / `key.PushCompPart(CompId)` (both take interned ids; `PushTypePart` inlines an unresolved one); `TypeKey::CachedHead/CachedParams/CachedBare` -> `_CachedHead/_CachedParams/_CachedBare` (still public: making them private needs `ParamsNamedBy` as a friend, which crashes GCC); `PushPtr(void const*)` -> `PushSymbol(TypeSymbol const*)`.
- `RawCompNode` -> `RawCompKey`; mangler `MangleCompNode` -> `MangleCompKey`.
- `KeyIn(scope, t, given)` -> `RawTypeKey(t, given, scope)` (twin of `RawCompKey(expr, scope)`; scope last in both).
- 2026-10-07: `comp_generics::FoldCompExpr(expr, scope)` -> `Scope::FoldedCompAstOf(expr)`; `comp_generics::CompValueAst` inlined into `Scope::CompAstOf`; `CompMembers::Find` / `scopes::CompMemberIdOf` -> `comp_generics::FindCompMemberId` (called directly); `KeyWritten` -> `KeyStamped`; `Rewrite` folded into `RewriteCompKey`.
- 2026-10-07: new module `spp.analyse.scopes.substitution` (above `type_key` and `comp_key`): `GenericSubst`, `ExprSubst`, `SubstituteTypeId`, `SubstituteCompId`, the type rewriter (the `Rewriter` class, which parsed key words, is now the free function `RewriteTypeId(id, subst, subst_id)` over `HeadOf`/`ArgsOf`, with `RewriteArgsId`, `Splice`, `SpreadTypePack`) and the private `RewriteCompId(id, subst, subst_id)` (memoised per (id, subst) once readable; was `RewriteCompKey(node, subst)`, and before that `comp_key`'s exported callback form `RewriteCompKey(node, bound, is_pack, member)`; an empty subst only folds). (`type_key.cpp`'s word helpers were briefly `TypeKey` statics, then made private again once the rewriter stopped parsing words.)
- `TypeRef::Named` -> `FromId`; `TypeRef::OfKind` -> `ForKindCheck`.
- `PotentiallyGenerateGnSubstitutedPrototype` -> `FindOrMakeGnSubstitutedPrototype`.
- 2026-10-15 (N6.17, the engine's entries): `FindOrMakeGnSubstitutedPrototype` -> `FindOrMakeFnInstance`;
  `InstantiateOverload` -> `RequireFnInstance`; `FindInstantiatedOverload` -> `FindRequiredFnInstance`;
  `CreateGnSupScope` -> `MakeSupInstance`; `CreateGnClsScope` (returned `Scope*`) -> `FindOrMakeClsInstance`
  (returns `TypeSymbol*`, memo check inside); `MakeInstance` folded into `InstantiateForScope`;
  `monomorphization::IsInTemplate(scope)` -> `scope.IsInTemplate()`.
- `RecordWrittenType` / `RecordTypeParts` / `RecordWrittenComp` / `RecordCompParts` -> `StampType` /
  `StampTypeParts` / `StampComp` / `StampCompParts` (`RecordedArgsFor` unchanged). `TypeFwdEq` kept.

**How to apply:** read old names in memories/docs through this map; name new code with the right column.
