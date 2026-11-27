---
name: comp-time-builtin-needing-meta
description: "A comp-time builtin that needs a mutable ScopeManager or meta (overload lookup) is a CompTimeFn subclass of its own, using Sm and Meta"
updated: 2026-11-25
---

The generic `ct::MakeCompTimeFn` wrappers pass a compile-time builtin only `ScopeManager const &` plus its generic
and value arguments. Anything that resolves overloads (`drop_utils::NeedsDrop` -> `GetAllFnScopes`) needs the
mutable `ScopeManager` and the `CompilerMetaData`. Since 2026-11-25 the `CompTimeFn` base holds both (`Sm`, `Meta`,
set by `PreloadGns`), and `std.mem.ops.needs_drop` is a `CompTimeFn` subclass (`NeedsDropCompTimeFn` in
`codegen/builtins.cpp`) whose `Invoke` reads `GnTypeArgs[0]`, `*Sm`, `Meta`.

**Why:** widening every `DetermineCompTimeFnSig` specialisation for one builtin would touch every comp-time
intrinsic; and putting the subclass in `analyse.utils.comp_time_intrinsics` would make it import `drop_utils`, which
risks an import cycle. `builtins.cpp` (codegen) already depends on both.

**How to apply:** a new comp-time builtin with that need follows `NeedsDropCompTimeFn`: a struct in the file's
`namespace { }` types block, `Invoke` inline, registered with `MakeUnique<...>()` as its `cmp_impl`.
