---
name: rel-profile-miscompiles-with-intrinsics
description: "Fixed - a GenOnce yielding a borrow of a temporary needs its allocas to declare their own lifetime"
---

Fixed 2026-09-05. `Str::from(7_u32)` printed nothing under `-m rel`: `sppc_memcpy(dst, undef, undef, 0, 0)`.

**Why:** a `GenOnce` that yields a borrow is desugared into a plain subroutine returning `T*`
(`GenExpressionAst::Stage11_CodeGen`, the `KW_FUN` branch). That is sound unless the body *builds* the thing it
yields - `Arr::fwd_ref`, `View::slice_ref` construct a `View` and yield its address, which after desugaring is a
pointer into a dead frame. `_ForceInlineBorrowedYield` already existed for this and is right as far as it goes:
`alwaysinline` puts the alloca in the caller. What defeated it is that the inliner then brackets every alloca it
moves with `llvm.lifetime.start/end` scoped to the inlined body - the exact scope the borrow must outlive - so the
caller reads storage llvm has been told is dead.

**How to apply:** the other half is `_DeclareBorrowedYieldStorage`, which emits `lifetime.start` and *no* end on the
lowering's entry allocas. `InlineFunction` skips an alloca that already has markers ("no need to add redundant, less
accurate markers"), so this stops it narrowing them. It must run after `Stage11_CodeGen`, not beside the attribute at
`Stage10_PreCodeGen` - there are no allocas until the body exists, and `getEntryBlock()` on a bodyless function
segfaults the compiler. This replaced a module-wide lifetime-marker strip; markers now survive and are honoured
normally. Bug was invisible until `llvm-intrinsics-unusable` was fixed, because unnamed intrinsics were inert.
