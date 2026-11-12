---
name: opaque-struct-layout-cache
description: "never size an opaque (registered, unfilled) struct: LLVM caches an empty layout per module forever; GetLlvmTypeOf now finishes placeholders; Opt[BigInt] had a [0 x i8] payload"
updated: 2026-11-12
---

`RegisterLlvmTypeInfo` gives a class an empty named struct, which `FillLlvmLayout` fills in. Some paths register a
class and use it without filling it: the array branch registers its element (a module-level `cmp` reached it first),
and the integer and float literals register their type. Until 2026-11-12, `GetLlvmTypeOf` (`LlvmTypeOfSymbol`) only
lowered a type whose LLVM type was null. It handed out a registered placeholder as it was, until that class's own
module reached stage 10.

Asking LLVM for the size or alignment of an opaque struct (`SortMembersForSppLayout`, a variant's payload) is
undefined. A release LLVM (no assertions) computes an empty layout and caches it in that module's `DataLayout` for
good, even after the struct gets a body. So every later size or offset in that module read 0. The trigger in `./project` (found by shrinking it) was
`cmp glob_array_1: Arr[BigInt, 100_uz] = ...` in an early module. Seen in `./project`:
`Opt[BigInt]` came out as `{ i64, [0 x i8] }` in two modules, so storing a value in it overran the stack frame
("stack smashing detected"; `tests/codegen/check-cross-module-layout.sh` reproduces it), and `BigInt` loads and stores
used `align 1`. The LTO module has its own `DataLayout`, so the backend never noticed. Full debug
info exposed it, reading struct offsets from the per-module cache (fatal: "Cannot implicitly convert a scalable
size").

**Fix:** `LlvmTypeOfSymbol` finishes a struct that is still opaque, not only a null type. `SortMembersForSppLayout`
never asks LLVM about an unsized type (one still being laid out further up the stack sorts last).

**Why:** a struct's layout is cached per module the first time anything sizes it, and never refreshed.
**How to apply:** guard every `DataLayout` size, alignment or offset query with `isSized()`. Find who sizes an opaque
struct with a gdb breakpoint on `llvm::StructLayout::StructLayout(llvm::StructType*, llvm::DataLayout const&)` if
`((*(unsigned*)((char*)$rsi+8)) >> 8 & 1) == 0` (no body): it should never fire.
Related: [[debug-info-line-tables]].
