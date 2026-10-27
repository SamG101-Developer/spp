---
name: pure-lookup-blocked-on-typeid
description: "Why Scope lookups still instantiate (OnInstantiationMissing): 90% of hook hits come from InstanceIdentityKey, and a structural key needs defaults/aliases applied first - i.e. TypeId (audit F6)"
updated: 2026-09-28
---

Measured 2026-09-28 while attempting audit F5 ("pure lookup"). One std compile fires `Scope::OnInstantiationMissing` about 2,600 times, all successfully.
- 2,330 of those come from `Scope::InstanceIdentityKey`. Keying an argument resolves it to a symbol, which mints the nested instance just so its address can go into the key.
- The rest come from `Assignable`/`TypeEq` over signatures, `aliases::TargetOf` and `TemplateOf`.

Keying instance arguments structurally (template plus the args' key, without resolving) does not work yet. An instance's recorded `ArgsId` includes solver-filled defaults (`Vec[S32]` is keyed with `A=GlobalAlloc`) and substituted alias targets, and a written, unanalysed argument has neither. So both keys would disagree, giving duplicate instances or failed lookups.

**Why:** F5 is folded into F6 (`TypeId`). Once a type is resolved exactly once, with defaults and aliases applied, the key *is* the resolved type and lookups can be pure.

**How to apply:** don't key *written, unanalysed* arguments structurally. Key through stamps or resolved instances instead, as `TypeKeyIn` does. To see who triggers the hook, break on `InstantiateForScope` by address (gdb can't resolve the module-mangled name) with `commands; bt 16; continue`, then group by the first frame outside the Canon/FindTypeSymbol machinery.

Related: F3 exposed that "S32 or Bool" and "Bool or S32" were two instances. Variants are now normalised to a canonical member order in `TypeIdentifierAst::Stage7`; see [[typeeq-accepts-forwarding]] and [[resolved-types-generic-identity]].

**Update, same day - keys are pure now.** Keying a *written* argument structurally doesn't work, but keying through a stamp does: an open instance's *recorded* arguments already carry defaults, and re-keying them in the current scope is exactly what `Canon` does. `Scope::TypeKey`/`TypeKeyIn`:
- keys an instance as `Inst + template + key(args)`;
- keys an alias as its target, read in the alias's `LinkedScope` or `DeclScope`;
- keys a variant as its sorted member keys.

Hook calls went from about 2,600 to 588, none of them from keying. The rest are `RelaxedTypeEq` on sup attach, and `Assignable`/`TypeEq` in `SameSignature`, `CheckDoubleExtension` and constraints; those need symbols.

`TypeEq` by key is blocked on `Self`: `SameType` reads `Self` as the other side's class, while a key uses the spelling.

**Done 2026-09-29.** Lookup is pure: `FindTypeSymbol`/`Canon` answer null for an instantiation not made yet. `FindOrMakeTypeSymbol` (file-local in symbols.cpp, via `MakeOnRead`) is the single place one is made on reading a type. It is used by `TypeRef::Of`, `AliasTarget`, the constraint check and `CreateGenericSym`'s argument binding.
- Removed: `Canon`'s re-keying guard, and `InstantiateForScope`'s re-entry guard and its swallowed errors.
- Written-type `TypeEq` is now `TypeId` equality (`Scope::TypeIdOf`, interned `TypeKey`).
- For names lookup can't find, keying resolves them structurally (`RecordedArgsFor`: parameter order, defaults, alias targets substituted).
- `SameSignature` binds `Self`, the base block's parameters and the method generics itself (see [[spp-self-type-resolution]]).

**How to apply:** if a raw `FindTypeSymbol` caller starts getting null where it used to get an instance, it needs `FindOrMakeTypeSymbol`. Reading a type to use it is an instantiation point; looking one up is not. The last `check_abstract` stage gate is deliberate: codegen can't instantiate.

**TypeId migration, 2026-09-29.** Instances are filed by `TypeId` (`ArgsId`, `Instances`, `GenericSubstitution`). `SameType`/`Assignable` identity is `SameId` (`Scope::TypeIdOfSymbol`). `TypeIdOf` is cached per node, per scope, under `TypeLookupGeneration` and the stamp. Nested args and variant members are one `Id` word (`PushId`/`PushTypePart`); an unresolved part stays inlined so `HasUnresolved` propagates.
- Kept by design: stamps, the caches and generations, `InstanceIdentityKey` (now the args half of a TypeId), and `ReadSelfPair`.
- A memoised TypeId→TypeId substitution needs a TypeId-headed type, not an AST; that is the next foundation, not part of the migration.

**TypeId-headed types, phase 1 (2026-09-29).** `HeadOf`, `BareTypeId`, memoised `SubstituteTypeId(id, GenericSubst)`, `Scope::SymbolOf`, `TypeRef::Id`. SameSignature and the conflict check compare param ids via `IdSubstOf`.
- Validate each switch with a shadow run (log disagreements over all probes) before flipping it.
- Nothing symbol-keyed may be cached statically: the test harness runs many compiles in one process.
- Keys are interned for the process, so a key→key memo is safe.

**Phase 2 (2026-09-29).** `TypeRef` has a constructor now, so there are no aggregate builds. Every resolved reference carries `Id` (via `Of`, `OfHead`, `OfSym` or `OfResolved`), and identity compares ids. Keying symbols is left only for `ReadSelfPair`'s `Self`.
- Deriving `Sym` from `Id` is blocked: a shadow run found 115k disagreements.
- The blockers: variant instances filed by written args (duplicates), `Bound` vs `Param` keys for the same `T`, and alias instances / alias-headed `Sym` words.
- Next step: file instances by their full `TypeId`.
