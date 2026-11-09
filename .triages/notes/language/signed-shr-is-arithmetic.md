---
name: signed-shr-is-arithmetic
description: "Signed >> is arithmetic (sign-filling), unsigned logical; intrinsics::bit_shr is always logical, bit_ashr always arithmetic"
updated: 2026-11-09
---

Since 2026-11-09, `>>` and `>>=` on a signed integer are **arithmetic**: `-8 >> 1` is `-4`, as in C, Rust and Java.
On an unsigned integer they stay logical. Before, both were logical, so `-8 >> 1` was a huge positive number.

Where it lives: the `BitShr`/`BitShrAssign` sups and `bit_shr_checked`/`_overflow`/`_wrapping` are per signedness in
SPP-STL's `sized_integer_signed.spp` (calling `intrinsics::bit_ashr` / `bit_ashr_assign`) and
`sized_integer_unsigned.spp` (`bit_shr` / `bit_shr_assign`), not on the shared `SizedInteger` base. Compile time agrees:
`kBinaryOps`' `>>` entry is `ShiftRightOfType` (`comp_time_intrinsics.cpp`), which picks by the left literal's type.

**The intrinsics are not per signedness:** `intrinsics::bit_shr` is logical and `intrinsics::bit_ashr` arithmetic,
whatever the operand's type. Code after a sign mask calls them by name - `div_floor`'s `bit_shr(self ^ that, w - 1)`
(0 or 1), `total_cmp`'s `bit_ashr(bits, w - 1)` (0 or -1).

**Why:** the book's signed formulas (Hacker's Delight, `>>s`) and every port from C/Rust assume it. The old
`div_floor`/`div_ceil` were written for it (`d + c` with `c` the -1 an arithmetic shift gives) and were wrong under the
logical one; they now use the logical intrinsic by name, so they are right either way.

**How to apply:** a sign mask or a signed halving can use `>>` on a signed value; to get 0/1 from the sign bit of a
signed value, call `intrinsics::bit_shr` by name. Related: [[spp-full-width-shift-is-poison]],
[[checked-arithmetic-is-default]].
