---
name: dead-code-sweep-skip-types
description: types.ixx: library types are aliases, not wrappers; Vec is a std::vector alias since 2026-10-13 (N0.1) - containers use the standard API (emplace_back, size, empty)
updated: 2026-10-12
---

`headers/spp/utils/types.ixx` names the library types the compiler uses, so an implementation can be swapped in one
place. Decided 2026-10-12: every library type there is an **alias**, not a wrapper. `Vec` (today a ~250-line class
wrapping `std::vector`) is to become a plain alias, all `std::vector` uses become `Vec`, and the call sites move to the
standard method names (`emplace_back`, `empty`, `size`, ...), as `Map`/`Set` already use ankerl's. Often-used library
types get an alias too (`std::optional`, `std::filesystem::path`), and existing aliases are used everywhere
(`Str`, `Function`, `Pair`, ...). Plan: `../../../.triages/next-steps.md` section 0 (N0.1-N0.4).

**Why:** the maintainer: "we dont wrap ankerl map/set, just alias, i dont think we need to wrap the vector class either" and "if i
use an stl type a lot in the repo, its be good to alias it in types.ixx too, in case i want to replace its
implementation at some point". The earlier rule (2026-10-01, "leave the types file alone (vec etc)") protected the
wrapper's unused members from dead-code sweeps; the wrapper itself is now going.

**Done for `Vec` 2026-10-13 (N0.1):** `Vec<T, A = std::allocator<T>>` aliases `std::vector` (allocator kept for
arenas). Write `emplace_back`/`size`/`empty`/`back`/`append_range`; never reintroduce PascalCase container methods.
Our own classes keep PascalCase (`GenericSubst::IsEmpty`, argument groups' `At(key)`), except container-likes such
as `SupScopeList` (`begin`/`end`/`size`/`empty`). `std::ranges`/`std::views` are banned by a pre-commit hook: move a
range in with `append_range(genex::views::move(x))`, compare with `genex::equals`.

**Aliases since 2026-10-13 (N0.2/N0.3):** `Opt`/`Nullopt`, `Path`, `Arr`, `Span` (genex::span), `Str`/`StrView`,
`Func` (copyable_function<Sig const>, fallback std::function), `FuncRef` (function_ref, fallback Func), `Variant`,
`Monostate`. Standard tuple/variant functions are re-exported under their std names (`get`, `get_if`,
`holds_alternative`, `visit`, `apply`, `tie`, `ignore`, ...): types PascalCase, std functions snake_case (the maintainer). Use
`FuncRef` for a callable parameter only called during the call; `Func` when stored, captured or nullable. A
`Func<T(...)>` parameter can't deduce `T` (it goes through `_FuncOf`): spell `T` or take `FuncRef`. Outside namespace
`spp` (unnamed namespaces, `namespace std`) write `spp::Opt` etc.; inside `Module`, `spp::Path` (member named Path).
`llvm_pipeline.cpp` (was llvm_passes.cpp) imports spp.utils.types and uses Str (2026-10-19).

**How to apply:** library types keep their library's API; the rest of the code is PascalCase. N0.2-N0.4 (more
aliases, use existing aliases, drop `std::bit_shl`) are still open. Related: [[code-reduction-means-functions]],
[[spp-map-set-not-std-unordered]], [[spp-naming-abbreviations]].
