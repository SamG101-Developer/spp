---
name: hash-table-intrinsics
description: "Semantics of umul_wide/smul_wide, load/store_unaligned, bytes_eq/msb_mask, prefetch_read/write: half order, mask bit layout, index units, bounds"
updated: 2026-11-25
---

The hash-table intrinsics (added 2026-11-25 for SPP-STL's `HashMap`) were specified by their stubs alone, so the
compiler fixed these semantics (`llvm_fn_impls.cpp`, `SimpleWideMul`, `SimpleBytesMask`, `SimplePrefetch`,
`StdIntrinsicsLoad/StoreUnaligned`):

- `umul_wide`/`smul_wide(a, b) -> (T, T)` is **(low, high)** of the double-width product, Rust's `widening_mul`
  order. Lowered as a `mul` at 2W (zext/sext), which selects to one `mul`/`imul`/`umulh`.
- `load_unaligned`/`store_unaligned` take `index` in **bytes**, align 1, target byte order.
- `bytes_eq_mask[lanes]`/`bytes_msb_mask[lanes]`: **bit i is byte i** (SSE2 `pmovmskb` layout), zero-extended to
  `U64`. A `<lanes x i8>` load, `icmp eq` splat / `icmp slt 0`, bitcast to `i<lanes>`; 16 lanes select to
  `pcmpeqb`+`pmovmskb` on x86-64. `lanes` outside 1-64 is an ICE.
- `prefetch_read`/`prefetch_write[T, locality]` take `index` in **`T`s** (not bytes). `locality` is LLVM's 0-3 (0 =
  `prefetchnta`, 3 = `prefetcht0` on x86-64); above 3 is an ICE. Data cache, never faults.

**Why:** a std change that assumes the other half order or bit order miscomputes silently; the ICE bounds are safe
only because every stub but the wide multiplies is `!package`.

**How to apply:** keep std's hash code to these conventions, or change both sides together. The `!package` ones have
`EXCLUDED` reasons in `tests/codegen/intrinsics.py` (a user program cannot call them); to probe them, flip them (and
`NonNull.from_mut`/`erase_type`) to `!public` in a scratch std copy only. Related: [[codegen-checks-suite]].
