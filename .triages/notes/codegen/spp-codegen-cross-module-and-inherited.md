---
name: spp-codegen-cross-module-and-inherited
description: "Codegen invariants - globals mirrored per module, bodies emitted into ctx->Module (own walk), an inherited method is compiled per receiver"
updated: 2026-11-13
---

Both found by the `std::net` test pass (2026-09-06) and both fixed; the invariants are worth keeping in mind.

- **A global belongs to the module that defines it.** Reaching a `cmp` constant from another module has to go through
  `GetOrAddGlobalIntoCurrentModule`, or the LLVM verifier rejects both modules with "Global is referenced in a different
  module!". `LlvmMaterialize.cpp` and `identifier_ast.cpp` already did this; the *member access* path
  (`postfix_expression_operator_runtime_member_access_ast.cpp`, the symbolic-lhs branch) did not, so
  `SomeType::constant.field` from another module was invalid ir. Any new path that reads `sym->LlvmInfo->Alloca` needs
  the same `dyn_cast<GlobalVariable>` check.
- **A function body is emitted from its own module's walk**, an instantiation's from its template's
  (`_CodeGenGnSubstitutions`, with the owner's context), so "the current module" is always `ctx->Module`. Declare
  helpers, intrinsics and mirrored globals there. `GetEmissionModule` (the insert point's module) covered the time
  when it was not, and was removed 2026-11-10; subroutine and coroutine `Stage11_CodeGen` raise an Ice if a body's
  function is in another module.
- **A method reached by superimposition is compiled per receiver.** `PinSelfToReceiver` (`overload_resolution.cpp`)
  pins `Self` to the receiver for any function called on a type other than the one declaring it, whether it reads
  `Self` through a `self` parameter, its signature or only its body, so the monomorphiser mints a per-receiver copy,
  mangled `#[Self=<receiver>]#`; nothing is minted where the receiver *is* the declaring type. It replaced
  `SignatureNamesSelf`/`declared_on_abstract` on 2026-10-09 (open-bugs #19). Before that a borrowed `self` pinned
  nothing, so the body was compiled once against the declaring class: its field GEPs used the *base's* sorted layout
  (`SortMembersForSppLayout` orders each class's fields independently) and its calls resolved against the base's
  overload set, so an inherited `!virtual_method` default never reached the override. A by-value `self` that pinned
  nothing was invalid ir outright ("Call parameter type does not match function signature"), which cost `std::net`
  its `close(self)`/`drop(self)`. Pinned by `test_inherited_borrowed_self_method_reads_the_derived_layout` and
  `test_inherited_default_method_reaches_the_override` in `project/tst/regression/codegen.spp`. This is option A in
  `.triages/docs_open/dyn-dispatch-design.md`, and it is why a `dyn` vtable needs a slot for every method, not only
  abstract and virtual ones.

  An earlier fix reinterpreted the argument through memory at the call site instead, guarded on equal
  `getTypeStoreSize`. It worked only because a base contributing all the attributes gives an identical layout, and it
  declined (back to invalid ir) if a derived type ever declared its own field. Deleted in favour of the above - a
  silent byte-pun of mismatched types is the "analysis says yes, codegen quietly does something else" shape worth
  avoiding.

- **A `cmp` type annotation was the one position that never substituted `Self`.** `CmpStatementAst::Stage4_QualifyTypes`
  explicitly skipped qualifying a `Self` type, so the symbol carried the literal `Self` and could be read but not bound
  anywhere `Self` means something else - `IpV4Addr::localhost` would not go into an `IpAddr`. It now calls
  `ResolveAndSubstituteSelfType`, as params, return types, `self`, case patterns and `ret`/`gen` targets all do; that
  helper is a plain clone when there is no enclosing `Self`, so the old guard still holds in that case. Test:
  `regression::tst::codegen::test_a_self_typed_cmp_constant_is_usable_outside_its_sup_block`.

Also fixed there: `IntegerLiteralAst::Stage11_CodeGen` rewrote the `o` of an octal prefix to a zero and read the result
as decimal (a boost-era hack), so `0o4000` was 4000 rather than 2048 - while the constant-folding path `BigVal()` read
it correctly, so the two disagreed about the same literal. numex parses `0x`/`0o`/`0b` itself; pass the token as
written. Red-turned-green tests: `regression::tst::codegen::test_octal_literals_are_read_in_base_eight` and the three
beside it.

Related: [[llvm-info-standins-lose-field-map]], `net-module-findings` (SPP-STL notes), [[spp-cls-layout-reorders-fields]].

**2026-10-18:** `GetOrAddTargetIntoCurrentModule` (llvm_fn.cpp) used to create the mirror declaration with no
attributes; for an `!ffi` function (no definition anywhere) the LTO link kept that bare copy, losing `spp-ffi`. It
now copies attributes + calling convention - keep it that way when touching the mirror.
