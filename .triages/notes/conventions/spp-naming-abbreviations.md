---
name: spp-naming-abbreviations
description: "the maintainer's naming rules from cleanup 4.2/4.3: Gn/Fn/Cls/Symbol/Table/CompTime/Id-Key, the Get/Find/Read/Of verbs, PascalCase but locals, and the exceptions"
updated: 2026-10-02
---

Settled by the maintainer on 2026-10-02 (.triages/analyse-cleanup-review.md 4.2) and applied tree-wide:

- **Gn**: class names (asts, `GenericSubstitution`, `GenericSolver`, errors) and grammar `Parse*` / `k*Ast` keep
  "Generic"; everything else is "Gn" ("Generics" → "Gns"). Enum values are short: `TypeKind::GnTypeParam`,
  `VariableKind::GnCompParam`.
- **Fn**: ast and error names keep "Function"; everything else is "Fn", never "Fun"/"Func". Exceptions:
  `IsTypeFunction` (matches a type name), `TokFun` (the `fun` keyword), s++ type names (`FunMov`, `FunRef`).
- **Cls**: ast names keep "Class"; else "Cls" (`TypeKind::Cls`).
- **Symbol** in full, never "Sym", except local variables. A member must not share its type's name: Scope's are
  `LinkedTypeSymbol` / `LinkedNamespaceSymbol`.
- **Table**, never "Tbl".
- **CompTime** for comp-time evaluation; **Comp** for comp generics; **Cmp** only for the `cmp` keyword/statement.
- **Id** = an interned value (`...IdOf`); **Key** = what gets interned (`TypeKey`, `CompKey`, `ParseCompKey`). No
  "Identity" in names.
- **IsType*** spelled out (`IsTypeGenerator`, `IsTypeArray`, `IsTypeTuple`).
- **Verbs** (4.3): `Get` = accessor or filter; `Find` = search that may be empty and makes nothing; `FindOrMake` =
  makes what it does not find; `Read` = identity under bindings; `...Of` = pure identity function; `Substitute` =
  explicit substitution. No `Search` / `Lookup` / `Resolve` verbs. Aliases *link* (`LinkTo`), bindings *bind*
  (`BindTo`, `Rebind`).
- **Predicates** (4.4): containment is `Does<Subject>Name<What>`, a singular What taking "A" / "An" (`DoesArgNameAnUnboundPack`, `DoesTypeNameAGnParam`; plural or proper names take none: `DoesTypeIdNameParams`, `DoesTypeNameSelf`,
  `DoesArgNameAPack`); a thing's own state is `Is*` (`IsClosedTypeId`); every bool field is `Is*`. Ast / ref pairs are
  `X` / `XRefs` (`VariantMembers` / `VariantMemberRefs`, as `TypeArg` / `TypeArgRef`).
- PascalCase for every function, method and field (private: `_Pascal`); locals and params stay snake_case;
  `PascalCase_WithUnderscore` is left as is. Exception: utils/algorithms.ixx (a genex extension: `move_accumulate`,
  the `cast_unique` / `cast_shared` / `tuple_nth` views and their `*_fn` structs) and utils/ptr.ixx's casts
  (`dynamic_shared_cast`, `static_shared_cast`, ...) are snake_case, matching genex / `std::*_pointer_cast`.

**Why:** one form per concept; the maintainer chose each rule explicitly, including the exceptions.

**How to apply:** name new code by these rules. Pinned by the protected test file: `build_temp_project`,
`GetAllComptimeValues`. Related: [[symmetry-levels-up]], `cleanup-series-workflow`.
