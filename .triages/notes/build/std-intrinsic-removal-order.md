---
name: std-intrinsic-removal-order
description: "Removing an intrinsic std declares: keep its compiler registration until the SPP-STL change merges, or every gtest compile ICEs"
updated: 2026-11-06
---

The gtest harness and CI compile the upstream SPP-STL from GitHub, not a local checkout (see
[[spp-test-harness-stale-std]]). If the compiler drops a `Register(...)` for an intrinsic that upstream std still
declares, every test that compiles std fails with "Internal Compiler Error: compiler_builtin function '...' is not
registered in kBuiltinFuncs". On 2026-11-06, removing `std.mem.ops.drop_in_place` took 17 of 18 `TestMscVisibility`
tests red at once.

**Why:** std and the compiler land separately. A new intrinsic has to be in the compiler before std uses it, and an old
one has to stay in the compiler until std stops declaring it.

**How to apply:** add new intrinsics in the compiler first; remove old ones in three steps: the compiler keeps the old
registration (with a Todo), std drops the declaration, then the compiler deletes it. Test the std side locally with
`spp test` from the SPP-STL checkout, which compiles that checkout itself.
