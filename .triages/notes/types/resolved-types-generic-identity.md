---
name: resolved-types-generic-identity
description: "Since 2026-09-14 generics resolve by identity (stamps + Scope::Canon, ParamId/BindsParamId, instances keyed in the template's Instances map), not by spelling; the rules that each broke std when violated"
updated: 2026-09-15
---

The "resolved types" rewrite (2026-09-14, builds 88-122) replaced name-based generic resolution. Before it, a type
was an AST re-resolved by spelling in whatever scope read it, so a caller's `T` read inside a callee instance whose own
param was also `T` meant the callee's (a hygiene bug), and instances were keyed by `ToView()` text.

**Mechanism:**
- **Param identity:** `TypeSymbol`/`VariableSymbol::ParamId` (non-zero on params, `NextGnParamId()`) and
  `BindsParamId` (on bindings). `RegisterGnSymbols` records the scope's own param ids by name *before* registering
  carried-in externals, else a carried-in `T` evicts the instance's own (was E72 on `NonNull`).
- **Stamps:** `TypeAst::Stamp()` (TypeSymbol*) and `IdentifierAst::Stamp()` (VariableSymbol*), carried by `Clone`.
  `FindTypeSymbol`/`FindVarSymbol` honour a stamp via `Scope::Canon`/`CanonVar`: a param maps to this scope's binding
  with the matching id, a closed class to itself, an open instance is re-keyed from this scope.
- **Instance identity:** `Scope::InstanceIdentityKey(args)` -> `template->Instances[key]`, with `InstanceOf` and
  `ArgsId` on the instance. Function instances use `GenericSubstitution::ArgsId`.
- **A binding's `FqName` is what it is bound to** (else its `GenericVal`); `BoundName` was folded into it.
- `TypeEq`'s spelling match (unbound `T` == bound `T`) is **gone**; `drop(buffer)` in iterator.spp is the canary.

**Rules, each learned from a std break:**
- Never stamp a shared node. `FqName` stamps a dedicated clone of a param's `Name`, and `WithoutGns` must not
  inherit a stamp through the shared `_CachedWithoutGns` (was E83 AddAssign, E26 `self.buffer.alloc`).
- Any clone whose generic args are replaced must drop its stamp (and its last part's): `SubstituteGenerics`,
  `TypeUnary`/`TypePostfix::WithGns`, and anything assigning `GnArgGroup` on a clone. A cached `FqName` is
  stamped, so a clone of it with new args still names the old type. Each leak broke std or tests: recursion, and
  `Vec[Tup[U64, U32]]` reaching codegen as `Vec[T=Tup[Items=Items]]` via the tuple alias path in
  `FindOrMakeClsInstance`.
- Key a `Self` argument by spelling, not meaning: a signature keeps `Vec[Self]`, and in each nested sup scope `Self`
  is a new instance, so keying by meaning minted `View[T=Self]`, `IterRef[T=Self]`, ... until the stack overflowed.
- File an instance in `Instances` before analysing anything that can name it (runaway instance creation).
- Argument-rewrite sites (KW/positional Stage7, `GenericArgumentTypeAst::Stage4`) must not rewrite to a binding's
  value outside an instance body, or `Single[Arr[T]]` re-binds forever.
- Instances are shared across modules now, so one minted before its class's Stage5 visibility sync (use statements
  analyse targets in Stage4) was private everywhere. Stage5 syncs every `Instances` entry (E83 `Eq[Rhs=Rhs]`).

- (2026-09-15, step 8a) Type args are stamped, not rewritten to `FqName`. Stamp an alias arg as the ALIAS, never its
  target: at Stage 3 the target is still the template, so `Limits[U8]`/`Limits[U16]` collapse (E14). Leaving it
  unstamped fails too (E26, a `use` spelling not visible where read). Instead `CreateGenericSym` binds an alias as
  its `Alias->Resolved` target once that is an instance/closed class, since a binding snapshots `LinkedScope`
  (a Stage-3 alias links the template, and `Arr[T=S32]` never lowered).
- A `Canon` miss on an open instance calls `Scope::OnInstantiationMissing` (stages 5.5-9.5) ->
  `InstantiateForScope`: substitute with `SubstituteGenerics(scope.GetGns())`, never mutate a clone's nested
  args in place (segfault) and never canonicalise only the top level (E1, nested `T` left).

- (2026-09-15) Stage 4 qualifies nothing now (renamed `Stage4_ResolveDeclarations`; alias targets are stamped
  via `StampWrittenParts`). Instance names keep closed-bound args as written but stamped with the bound class
  (`StampClosedBindings`); `SubstituteGenerics` must skip a name stamped with a non-parameter, or
  `InstantiateForScope`'s by-spelling substitution rebinds it. Don't import the symbols module into
  compiler_meta_data.ixx (GCC fwd-decl clash); `meta` holds `Shared<TypeRef>`.

- `SubstituteSelfTypeWith` returns a plain clone when "Self" is absent, and a clone keeps the written node's access
  marks: using it for a call's return type made std's return types answer access checks at the call (E83 in
  socket.spp). A rebuilt substitution drops them. Symbol-side `TypeEq` is NOT pointer equality: distinct instances
  are TypeEq-equal today (unbound params, packs, variant/convention loosening in args, Self, aliases).

- (2026-09-15) Comp identifiers are stamped at Stage 7 and type args naming a binding are stamped with it (Canon
  resolves a GenericArg stamp by BindsParamId, else itself); comp carrying + the comp sweep are gone.
- (2026-09-15, later) The TYPE carry-in (`GetExtendedGenericSymbols`, `external_generic_syms`) is **gone too**, IR-
  neutral (probe 92/92, project 196/196). It was masking two things, not doing real work:
  1. `RecursiveAliasSearch` substituted an alias target's own args into itself (`HeapArr = Single[Arr[T, n], A]`
     became `Single[Arr[Arr[T, n], n], A]`); the carried `T` hid it by spelling. Fixed (substitute once, start empty).
  2. It collapsed open instances over different unbound params into one: every `Some[T=<param>]` became the one minted
     inside `sup [T: Copy] Some[T] ext Copy`, so template `Opt[T]` read as Copy and std's linearity bugs passed.
     Without it std needs `drop(...)` in `Opt::ior_/and_/op_as_residual`, `Res::pass_opt/fail_opt/zip` (SPP-STL).
  Dead ends: resolving sup pattern values in the class scope (S4) makes `Self` in `ext` args nest forever.
  Measure removals with an instance-set diff, not just errors.
- (2026-09-16) `TypeEq` has one implementation, on `TypeRef`s (`TypeEqCore`); the written-type overload and
  `TypeFwdEq` are adapters. Landed by parity-logging old vs new on both probes until only intended differences were
  left, then IR-verified. Rules it needed: two instances of one template with equal `ArgsId`s are one type;
  `Self` args compare by spelling; `InstanceIdentityKey` keys a written `Self` by spelling *before* resolving it
  (a `Self` binding to `$AsyncTarget` had keyed `Ord[Rhs=Self]` apart) and keys a positional arg to a fixed param by
  the param's name (an unanalysed `Some[Str]` missed `Some[T=Str]`). It is stricter than the old one: a bare
  binding (`That`) no longer equals any instance of its bound template regardless of arguments, which had matched
  `from(that: SizedIntegerUnsigned[..])` to the `From[SizedIntegerSigned[..]]` trait. Variant coercion
  (`CoerceToVariant`, `GetVariantIndexOfMember`) and `VariantMemberRefs` are ref-based too.
- (2026-09-16) **An instantiation is answered by identity, never by spelling.** The type table holds one entry per
  spelling, so `Mutex[T=T]` written in the class and in `sup [T: Drop] Mutex[T]` is one entry and two types - the
  block's `T` carries its own constraints. `FindTypeSymbol` used to fall back to that entry after an identity miss, so
  `T: Drop` was checked against the block's own `T` and held, attaching conditional `Drop`/`Copy` blocks to templates.
  It now rejects a by-spelling hit that instantiates the same template under another identity. Two more, same session:
  `Scope::Canon` guards re-entry, since an instantiation recorded under a name that names itself re-keyed forever; and
  a bare alias (`USize`, no written arguments) no longer matches any pattern (`U8`) by having nothing to compare.
  `RelaxedTypeEq` is now a hybrid - pattern as written syntax, matched side resolved - and only resolving a *whole*
  generic type exposes these; the old matcher only ever resolved heads.

**Still kept, deliberately:** `GenericVal` (only record of a param-to-param binding). `ResolveBoundGenerics`,
`WithoutSelfBindingGenerics` and `names_an_open_generic` are gone (checked 2026-09-15)
([[generic-leak-shared-instantiations]]).

Status (2026-09-14): the first full suite after the rewrite showed 25 regressions vs f69, in three groups: defaults,
`Self` in generic args, and tuples. All three were fixed (see the rules above). The final full suite (build 132) was
2301 passed / 34 failed (f69: 2292 / 43), with no new failures and 9 f69 reds fixed. The whole rewrite is recorded
in `docs/generic-resolved-types.md`. Compiles got slower: the median test went from 1-2 s to
5.9 s, suspected to be the `InstanceIdentityKey` string built on every uncached generic lookup. It had not been
profiled when this was written.
See [[generic-param-sym-links-dummy-scope]], [[spp-visibility-stage-ordering]].
