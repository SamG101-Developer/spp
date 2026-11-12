---
name: source-line-table
description: "crash-report lines come from a table built from spp.o's DWARF (llvm_lines), keyed by symbol-name hash + offset; never reference S++ fns from IR; LoadedObjectInfo for sections at 0"
updated: 2026-11-11
---

With `[binary.debug] info` not `"none"`, `spp.codegen.llvm_lines` reads the emitted `spp.o`'s DWARF
(`DWARFContext`), builds a table of every function's ranges and their inlining chains, and emits it as a second
object, `llvm/spp-lines.o`, holding the hidden symbol `__spp_lines`. `main` calls `sppc_set_source_lines(__spp_lines)`
first. sppc defines the format (`SOURCE LINES`, `backtrace/common.h`; version 1) and prints the chain under each
frame. Functions are keyed by the FNV-1a hash of their symbol name, as `_sppc_bt_resolve` returns it (Mach-O's `_`
dropped), plus an offset into the function, so the table holds no addresses. A stripped program has no names left,
so it gets no lines either.

**Traps:**
- **Never reference the S++ functions from IR** (a function-pointer array, say): an address-taken function gets a
  `bti c` landing pad (`AArch64BranchTargets`), which weakens BTI. That rules out a table of relocations against the
  functions themselves.
- **In an object every section starts at 0, and `DWARFContext` finds a compile unit by address alone**, ignoring
  `SectionIndex`. A function in `.text.unlikely.` (a rel build's cold functions) was looked up in the wrong unit.
  `SectionLayout`, a `LoadedObjectInfo`, gives each code section its own load address.
- **An inlined call need not start a line-table row**: a line-0 row runs on into it. Ranges are also cut at every
  `DW_TAG_inlined_subroutine`'s address ranges.
- **The table object copies the program's module flags**: the linker ANDs the BTI and IBT properties across objects.
- **Order of landing**: every spp build with lines links against `sppc_set_source_lines`. sppc and the SPP-STL bump
  land before spp merges, or every link fails with an undefined symbol.
- **Check the table against `llvm-symbolizer`**, not binutils `addr2line`, which stops at the innermost frame of a rel
  build. Verified 2026-11-11 against every instruction of `./project`, at dev and rel: 0 mismatches.

**Why:** sppc stays a thin kernel wrapper, with no DWARF parser in it (libbacktrace was tried and dropped). The table
reads the same on every platform, and DWARF stays the one record of where code came from.
**How to apply:** a change to the format changes `llvm_lines.cpp` and sppc's `common.h` together, with a new version.
`tests/codegen/check-debug-info.sh` covers it end to end, and sppc's `test_process` covers a hand-built table.
Related: [[debug-info-line-tables]], [[sppc-sibling-repo]].
