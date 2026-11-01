---
name: module-order-failures
description: "Some module orders crash or fail to compile std, today's code included; GlobSpp's sort hides it until a file is added or renamed"
updated: 2026-11-01
---

`GlobSpp` sorts, so a set of filenames always compiles the same way, and this stays hidden until adding or renaming a file
lands on a bad order. With a seeded `std::ranges::shuffle` after that sort, about 5-10% of orders of `./project` fail, each
one every time for a given binary. Failing seeds, of 43:

| Binary | Eager mocks (as before 2026-11-01) | Mocks attached on read |
|---|---|---|
| Plain | 4, 38 segfault; 35 E94 `raw_buf.spp:280` | 23, 35, 39 segfault; 9 E26 `vector.spp:485` |
| `AttachSpecificSuperScopesImpl` loops over a copy | 4, 9 segfault | 39 abort; 35 E94 `raw_buf.spp:280` |

Every seed where both modes pass has identical IR between them (39 of 43 in the second row).

**Why:** a heap-use-after-free, found by ASan on every failing seed in both modes. `AttachSpecificSuperScopes` held
`&it->second` into `NormalSupBlocks`, a dense map with its values in one flat array, and `MakeSupInstance` adds to that map
through `NormalSupBlocks[mock]`, which frees the array as it grows. A nested attach reaches that before the loop starts
(`TypeIdOf`, `_CanonType`, `GetSupScopes`, the read hook), which is why the copy in the second row, taken at the loop, did
not cure it. Copying the list at the lookup left 9 of 9 previously failing runs clean under ASan, and the `E26` and `E94`
went with it: they were this bug too, not ordering logic.

**How to apply:** never hold a reference into `NormalSupBlocks` (or any `Map`) across anything that can attach or
instantiate. Do not read one passing order as proof a sup-attachment change is safe. To compare modes, put the shuffle and
the old behaviour behind non-`SPP_` environment variables in a throwaway binary (`SPP_` names stop spp at startup) and
compare the same seed both ways, as different orders' IR differs anyway. See [[sup-attach-fixed-point]].
