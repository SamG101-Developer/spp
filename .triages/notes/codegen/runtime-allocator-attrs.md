---
name: runtime-allocator-attrs
description: sppc_malloc/calloc/aligned_alloc/realloc/free are declared as LLVM allocators; they must stay plain libc wrappers
updated: 2026-11-11
---

`ApplyRuntimeAllocatorAttrs` (`../sources/spp/codegen/llvm_entry.cpp`, called in `compiler_link.cpp` on the linked
module before the optimiser) gives every declaration of sppc's five allocation functions LLVM's allocator attributes:
`allockind` (alloc/realloc/free, uninitialized/zeroed/aligned), one `"alloc-family"="sppc_malloc"`, `allocsize`,
`allocptr`, `allocalign`, and a `noalias` return. LLVM then treats them as `malloc` and `free`: a pair whose memory
nothing else sees is removed (InstCombine), fresh memory is known not to alias, and `allocsize` gives
`dereferenceable_or_null`. It needs a `ptr` return and `ptr` parameter, so it only works with `Ptr[T]` lowered to `ptr`
([[nonnull-lowers-to-ptr]], [[ffi-struct-params-c-abi]]); the compiler assumes a std that has it (an `i64` signature
would fail LLVM's verifier).

**Why:** O14 (2026-11-09). An unused `Vec` allocation in a loop kept its `sppc_malloc`/`sppc_free` pair without the
attributes and loses it with them.
**How to apply:** the attributes promise LLVM that these functions do nothing but allocate and free: it removes and
merges calls freely. If sppc's allocators ever gain a side effect that must happen (counting, poisoning, logging, a
hook), that promise breaks; drop the attribute or move the effect. A new sppc allocation function goes in the table
with its own kind. Memory from one family must only be freed by the same family (libc's `free` is another family).
