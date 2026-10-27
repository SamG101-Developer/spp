---
name: nonnull-lowers-to-ptr
description: "NonNull[T] must lower to a bare llvm ptr (is(*NON_NULL) special case), not the generic struct-of-attrs path"
updated: 2026-07-29
---

`NonNull[T]` (project/vcs/std/src/std/mem/pointer.spp) declares zero explicit fields in its `cls` body — all its
behavior comes from `!intrinsic`-tagged methods in `sup` blocks. The compiler's generic struct-layout path
(`ClassPrototypeAst::_FillLlvmLayout` in sources/spp/asts/class_prototype_ast.cpp) only looks at `cls`-body attributes,
so without a special case `NonNull[T]` becomes a genuinely empty, zero-size LLVM struct — unusable, since `View[T]`
and `RawBuf[T, A]` both store a `ptr: NonNull[T]` field and the existing (already-written) codegen for
`View::index_ref`/etc. already assumes that field is pointer-width.

Fixed by a special case in sources/spp/codegen/llvm_type.cpp's `RegisterLlvmTypeInfo` — now `is(*NON_NULL)`
(`TypeRef::IsA` against the precompiled template; N2.5 replaced the old `kNonNullParts` scope-path compare) — lowering
directly to `llvm::PointerType::get(...)` instead of falling through to the attribute-based struct path.

**Why:** `Ptr[T]` (the sibling class) legitimately has an explicit `addr: USize` field and becomes `{i64}` via the
normal path — only `NonNull[T]` needed the special case, since it's meant to be bit-identical to a raw pointer with no
visible field.

**How to apply:** When implementing any other `NonNull[T]` intrinsic (`from_func`, `erase_type`, `is_aligned`, etc.) or
touching `View`/`RawBuf`/anything embedding `NonNull[T]` as a field, remember self-by-borrow needs a load through the
borrow *plus* a second load to read the address `NonNull` itself wraps (no struct GEP — there's no field to GEP into).
self-by-value (e.g. `read(self)`, `cast(self)`) only needs the second load, since the by-value slot already holds the
pointer bits directly. See `StdNonNullRead`/`write`/`raw`/`cast`/`from_ptr_inner`/`fwd_mut`/`fwd_ref` in
sources/spp/codegen/llvm_fn_impls.cpp for the worked examples, and `SimpleCoroNonNullFwd` for the coroutine
(`fwd_mut`/`fwd_ref`) shared shape.
