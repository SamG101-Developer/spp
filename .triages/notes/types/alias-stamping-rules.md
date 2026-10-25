---
name: alias-stamping-rules
description: "How written-type stamps interact with aliases/`use`; the 2026-10-02 ./project breakage chain (U8 Self = template, Str[A=A], self-stamped use aliases) and the rules that fixed it"
updated: 2026-10-02
---

`RecordWrittenType` stamps a plain name with `WrittenTypeIdOf(sym)`; an alias stamps as **itself** (its address), which
is what makes a module-private `use` name (`Vec`'s default `A = GlobalAlloc`) readable from any module. Removing that
(skipping aliases) broke `GlobalAlloc` / `U32` lookups elsewhere — don't.

Rules that make the self-stamp safe (all landed 2026-10-02 while getting `./project` to build after the cleanup series):
- **Key-time follow**: `KeyWritten` keys a stamped alias head via `KeyAliasTarget`, and `KeyAliasTarget` follows a
  target that is itself an alias (`AliasHeadOf`, scope.cpp). A stamp taken before the alias's statement resolved (a
  `use` read first, module order) would otherwise name the alias forever.
- **No stamp for a bare template or a `use` of one** (`sym.UseTarget()->IsBareTemplate()`): `type MyNewType = Str`
  reaches `Str` through the prelude's `use`, and a stamp made `TypeIdentifierAst::Stage7` early-return before filling
  the `A=GlobalAlloc` default → `Str[A=A]`.
- **`RecordTypeParts` resolves each written type whole** (namespace-aware). The old walk resolved `std::number::U8`'s
  last part as bare `U8` in the current module, which is the `use` alias itself → self-stamp → `AliasTarget` loops.
- **`AliasCopyIn` matches by `Alias->Stmt`, not name node**: a `use` alias shares its target's name node, so a lookup
  of the `type` alias from a module that `use`s it returned the `use` alias.
- **`self_type::RegisterSelf` follows an alias** (`AliasTarget`): `sup U8 {}`'s Self is `SizedInteger[8, false]`, not
  the template the alias links (alias `LinkedScope` is the end-of-chain template by design).

Also found the same day: `TypeRef::Of(id)` on a `Self`/binding-headed id is empty and drops the id, so
`.Substitute` after it does nothing — `SameSignature` now compares substituted return ids first (every `Copy` type
was "abstract"). Private sup-block members are visible from the type the block is over (visibility normalises a sup
scope to its Self's scope).

**How to apply:** when a name resolves to the alias/template instead of the instance, check the stamp
(`LastTypePart()->WrittenTypeId()` head kind 7 = `Symbol`) before anything else. Related: [[use-imports-are-alias-symbols]],
[[alias-names-read-where-written]], [[pure-lookup-blocked-on-typeid]].
