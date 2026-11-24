---
name: special-lowering-needs-matching-layout
description: A type lowered specially (bare pointer, scalar) must be measured the same way in codegen::LayoutOf; Ptr[T] measured 0 bytes, so Vec never allocated for a class holding one
updated: 2026-11-23
---

`size_of` (and every `RawBuf` decision built on it) comes from `codegen::LayoutOf` (llvm_size.cpp), not from the LLVM
type. `LayoutOf` walks a class's attributes unless it special-cases the type. So a type that `RegisterLlvmTypeInfo`
(llvm_type.cpp) lowers to something other than a struct of its fields has to be special-cased in `LayoutOf` too, or
it measures as its fields.

`b062ec24` (2026-11-11) lowered the field-less `Ptr[T]` to a bare pointer next to `NonNull`, but `LayoutOf` still
special-cased only `NonNull`. So `size_of[Ptr[T]]` was 0, and so was any class holding a `CVoidPtr`.
`RawBuf::is_storage_free` (`size_of[T]() * cap == 0`) then took such a class as zero-sized: `Vec` never allocated
for it, and `append` stored the 8-byte value through the empty buffer's dangling `0x1`. That was every shield test that
built a `LibCtx` (`Vec[Handle[ProviderKind]]`), as `SIGSEGV at address 0x1`. Fixed by
`type_predicates::IsTypePointer`, used by all three of lowering, debug info and layout.

**Why:** lowering, debug info and layout each decide a type's shape. A rule written out in each drifts, and only
layout's copy is invisible: a wrong size is no build error, only wrong allocation at run time.

**How to apply:** a type with a special lowering gets one predicate, used in `RegisterLlvmTypeInfo`, `DebugTypeOf` and
`LayoutOf`. Check a new lowering with `tests/codegen/check-pointer-layout.sh`-style run tests, not only IR. A gtest
compiles and verifies but never runs the program. Related: [[opaque-struct-layout-cache]].
