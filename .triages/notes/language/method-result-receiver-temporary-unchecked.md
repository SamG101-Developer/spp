---
name: method-result-receiver-temporary-unchecked
description: "A call's owned result is a temporary even though PlaceOf derives a method call from its receiver; the hole let s.to_reverse().len() leak (fixed 2026-11-10)"
updated: 2026-11-10
---

`regions::PlaceOf` treats a method call on a place as a place (`v.clone()` is `v` plus one opaque step), which is
right for the borrows a call hands back (`v[mut i]`, a `fwd_ref`). `linear_utils::IsLinearTemporary` used to read
"has a place" as "is no temporary", so a non-`Copy` value *returned* by a method escaped every borrowed-temporary check
(E94, [[spp-linear-types]]): `s.to_reverse().len()`, `&addr.storage()` as an argument, `loop c in
self.to_reverse().chars()`. Nothing dropped those values. Found by valgrind (the three leaking `std::tst::fs::path`
tests went through `StrView::rfind_if`).

**Fixed 2026-11-10:** `IsLinearTemporary` skips the `PlaceOf` test when the expression is itself a call
(`PostfixExpressionOperatorFunctionCallAst`); a borrow handed back is still excluded by `IsBorrowed`. Tests in
`test_lin_temporaries.cpp` (`*_method_result*`, `test_valid_borrow_handed_back_by_a_call_is_no_temporary`).

The std side, which has to be on SPP-STL master **before** this compiler change merges (it compiles under the old
compiler too; the reverse of [[merge-spp-before-stl-for-new-features]]): `rfind_if` binds the reversed `Str` with
`defer drop`; `Ptr[T]` and `sppc::CSockaddrStorage` became `Copy` (the latter declared in `std/net/socket.spp`,
since the sppc stub generator emits no `Copy`); five test asserts on a non-`Copy` `Res` bind, assert and `drop`.
`benchmarks/std.rev` needs bumping to that std commit, as the pinned std's `rfind_if` is now E94.

**Why:** a leak the linearity rules exist to forbid, reachable only when the temporary came from a method call.

**How to apply:** a helper that asks "does this expression name a place" for ownership must treat call results
separately from field and deref chains.
