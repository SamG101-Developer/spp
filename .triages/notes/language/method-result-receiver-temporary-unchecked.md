---
name: method-result-receiver-temporary-unchecked
description: "A call's owned result is a temporary even though PlaceOf derives a method call from its receiver; drop_utils::IsTemporary skips PlaceOf for calls so the result is dropped"
updated: 2026-11-24
---

`regions::PlaceOf` treats a method call on a place as a place (`v.clone()` is `v` plus one opaque step), which is
right for the borrows a call hands back (`v[mut i]`, a `fwd_ref`). `IsLinearTemporary` (now `drop_utils::IsTemporary`) used to read
"has a place" as "is no temporary", so a non-`Copy` value *returned* by a method escaped every borrowed-temporary check
(then E94, under the linear rules): `s.to_reverse().len()`, `&addr.storage()` as an argument, `loop c in
self.to_reverse().chars()`. Nothing dropped those values. Found by valgrind (the three leaking `std::tst::fs::path`
tests went through `StrView::rfind_if`).

**Fixed 2026-11-10:** `IsTemporary` skips the `PlaceOf` test when the expression is itself a call
(`PostfixExpressionOperatorFunctionCallAst`); a borrow handed back is still excluded by `IsBorrowed`. Tests in
`test_drop_temporaries.cpp` (`*_method_result*`, `test_valid_borrow_handed_back_by_a_call_is_no_temporary`).

The std side, which has to be on SPP-STL master **before** this compiler change merges (it compiles under the old
compiler too; the reverse of [[merge-spp-before-stl-for-new-features]]): `rfind_if` binds the reversed `Str` with
`defer drop`; `Ptr[T]` and `sppc::CSockaddrStorage` became `Copy` (the latter declared in `std/net/socket.spp`,
since the sppc stub generator emits no `Copy`); five test asserts on a non-`Copy` `Res` bind, assert and `drop`.
`benchmarks/std.rev` needs bumping to that std commit, as the pinned std's `rfind_if` is now E94.

Under affine ownership (2026-11-23, [[affine-ownership]]) the same test decides which temporaries are dropped at
their statement's end, so missing a call result there is a leak again, not an error. The std-side notes above are
history: those `defer drop`s are gone.

**Why:** a leak, reachable only when the temporary came from a method call.

**How to apply:** a helper that asks "does this expression name a place" for ownership must treat call results
separately from field and deref chains.
