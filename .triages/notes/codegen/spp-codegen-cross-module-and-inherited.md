---
name: spp-codegen-cross-module-and-inherited
description: "Two codegen invariants - globals must be mirrored per module, and an inherited by-value self needs a type bridge"
---

Both found by the `std::net` test pass (2026-09-06) and both fixed; the invariants are worth keeping in mind.

- **A global belongs to the module that defines it.** Reaching a `cmp` constant from another module has to go through
  `GetOrAddGlobalIntoCurrentModule`, or the LLVM verifier rejects both modules with "Global is referenced in a different
  module!". `LlvmMaterialize.cpp` and `identifier_ast.cpp` already did this; the *member access* path
  (`postfix_expression_operator_runtime_member_access_ast.cpp`, the symbolic-lhs branch) did not, so
  `SomeType::constant.field` from another module was invalid ir. Any new path that reads `sym->LlvmInfo->Alloca` needs
  the same `dyn_cast<GlobalVariable>` check.
- **A method reached by superimposition is generated once, for the type that declared it** - unless its signature
  names `Self`, in which case `SignatureNamesSelf` (in `overload_resolution.cpp`) pins `Self` to the receiver and the
  monomorphiser mints a per-receiver copy, mangled `#[Self=<receiver>]#`. A **by-value `self` now counts as naming
  `Self` too**: without that, a call on a superimposing type handed the derived struct to a signature naming the base
  one, which is invalid ir ("Call parameter type does not match function signature"). It cost `std::net` everything -
  `close(self)`/`drop(self)` are the only consuming operations on a socket, so under linear typing no socket could be
  disposed of at all. A borrowed `self` needs nothing (an opaque pointer either way), and the inner
  `not TypeEq(declared_self, receiver)` guard means nothing is minted where the receiver *is* the declaring type - so
  the cost is one extra copy per (by-value-`self` method x superimposing type) and zero otherwise. In `net` that is
  `close` and `drop` across three socket types: six functions out of ~2150.

  **Correction (2026-09-25): "a borrowed `self` needs nothing" is wrong.** The pointer is opaque, but the body is
  still compiled once against the declaring class, so (a) its field GEPs use the *base's* sorted layout while the
  derived struct is sorted independently - a base accessor on a derived receiver reads the wrong offset - and (b) a
  call inside it resolves against the base's overload set, so an inherited `!virtual_method` default never reaches the
  derived override. Both are reachable only when the base declares **no** abstract method, because
  `declared_on_abstract` in `PinSelfToReceiver` otherwise mints the per-receiver copy anyway. Confirmed in the emitted
  ir, not by running. Red tests in `project/tst/regression/codegen.spp`
  (`test_inherited_borrowed_self_method_reads_the_derived_layout`,
  `test_inherited_default_method_reaches_the_override`), Todo at `SignatureNamesSelf`, background in
  `.triages/docs_open/dyn-dispatch-design.md`.

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
