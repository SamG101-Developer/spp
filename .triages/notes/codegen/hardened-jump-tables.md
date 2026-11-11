---
name: hardened-jump-tables
description: spp builds its own jump tables (llvm_jump_tables): default in slot 0, index masked branch-free, asm barrier; LLVM never builds one
updated: 2026-11-10
---

With `[binary.codegen] jump_table_bounds` on (the default), `ApplyHardenedJumpTables`
(`../sources/spp/codegen/llvm_jump_tables.cpp`, after the optimiser) stamps `no-jump-tables` on every function, so
LLVM never builds an unprotected table, and lowers each dense switch (at least 4 cases and 10% of its range, LLVM's
defaults, constants at the top of the file) to a table of block addresses and an `indirectbr`:
- **Slot 0 is the default**, value `min + i` is slot `i + 1`, and the index is `zext(cond - min) + 1`, which never
  wraps. Out of range, it is masked to 0 with no branch: `mask = sext(idx u< N + 1)`, `idx & mask` (`cmp`, `sbb`,
  `and` on x86). The backend folds the `+ 1` into the table's address.
- **The mask goes through an empty inline asm** (`"=r,0"`). Without it the backend can see `idx & sext(cmp)` as a
  select, make it a `cmov`, and turn that into a branch (x86's cmov conversion), which brings the speculative
  out-of-bounds jump back.
- **No mask when the value cannot be out of range:** `computeConstantRange` proves it within the cases, or the cases
  cover the whole type.
- An `unreachable` default gets a trap block in slot 0, so a speculative path through it lands nowhere harmful.
- WebAssembly has no `indirectbr`: switches there stay compares.

**Why:** the first version built every x86 switch as compares (LLVM has no x86 jump-table hardening), which cost
case_expression 22% and bytecode-vm 10%, and failed the build on every target but x86 and AArch64. The masked
tables are as fast as LLVM's unprotected ones or faster (case_expression 521 ms, bytecode-vm 105, against 548 and 102)
and run everywhere (2026-11-10).
**How to apply:** a change to switch lowering, block layout or the hardening order must keep the table pass after the
optimiser (nothing may fold the table or the mask away). `tests/codegen/check-jump-table-bounds.sh` checks x86's
object for no LLVM tables and a masked table jump; the cross-codegen lane runs it on the other targets under qemu.
Todo in the file: relative (4-byte) entries instead of absolute ones.
