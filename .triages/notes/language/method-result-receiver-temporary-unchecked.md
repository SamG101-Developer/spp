---
name: method-result-receiver-temporary-unchecked
description: "OPEN 2026-11-10: s.to_reverse().len() compiles and leaks; the borrowed-temporary check is skipped when the receiver's outermost symbol is found through a call"
updated: 2026-11-10
---

Borrowing a non-`Copy` temporary is an error (E94, [[spp-linear-types]]): `Str::from("a").len()` is rejected. But
`s.to_reverse().len()`, and `loop c in self.to_reverse().chars()` in std's `StrView::rfind_if`, compile, and the
owned `Str` is never destroyed (no drop in the IR, on any exit). `FunctionCallArgumentGroupAst` looks up
`FindVarSymbolOutermost(*arg->Val)`, which walks `s.to_reverse()` back to `s`, and
`borrows::ValidateUnnamedArgumentBorrow` returns early for any argument with a symbol, before its
`IsLinearTemporary` check. Found by valgrind: the three leaking `std::tst::fs::path` tests all go through
`rfind_if` (14 bytes, a copy of `"/usr/local/bin"`, allocated in `Path::parent`).

**Why:** a leak the linearity rules exist to forbid, reached only when the temporary comes from a method call.

**How to apply:** the check should ask `IsLinearTemporary` (which uses `regions::PlaceOf`, and a call result is no
place) whatever the outermost symbol. Expect std code that relies on the hole (`rfind_if` first) to become E94 and
need a named binding plus `drop`.
