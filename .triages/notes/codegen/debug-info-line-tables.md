---
name: debug-info-line-tables
description: "[binary.debug] info = lines: llvm_debug gives each module a CU, each body a subprogram, each statement a location; verify objects with llvm-dwarfdump 23, not executables; no .debug_names"
updated: 2026-11-12
---

`[binary.debug] info` (`"lines"` by default, `"none"` byte-identical to no debug info) gives every module DWARF line
tables (`spp.codegen.llvm_debug`, debug-info plan A1): `StartDebugInfo` makes the module's compile unit (language
`DW_LANG_Rust`, as lldb shows no variables for a vendor code: plan B4) when `compiler_boot` makes its context, `FinishDebugInfo` runs before
the module is verified, `AttachSubprogram` gives each body a subprogram (subroutine, coroutine, closure; the
closure environment's destructor artificial at line 0), and `SetDebugLocation` sets the builder's location at each
body's entry and before each statement (`InnerScopeAst`, `InnerScopeExpressionAst`). Positions become lines through
`ErrorFormatter::LineColOf`.

**Traps:**
- **Each block (`InnerScopeExpressionAst`) opens a lexical block** (`EnterDebugBlock`/`LeaveDebugBlock`), tagged
  with its function, as a closure's body is generated inside its parent's block; locations and variables go in the
  innermost block of the builder's function. Leaving restores the location current at entry: otherwise the code after
  a `case` (storing its value) carried the last branch generated, and stepping showed the branch not taken.
- **A movable binding is `#dbg_value`-described, not declared** (`DeclareVariable` decides: not `Copy`, not a
  borrow; `LlvmVarSymbolInfo::DiVariable` set). `ResumeVariable` after its `let` stores and after an assignment,
  `EndVariable` at `IdentifierAst`'s value load, which for such a binding is always a move. Mixing a `#dbg_declare`
  and `#dbg_value`s for one variable is undefined, so a binding is one or the other. A new place that gives a binding
  a value (destructuring into an existing one, say) must call `ResumeVariable`, or it stays not available.
- **A new `llvm::DI*` class must be exported from `headers/wrappers/llvm.ixx`** first (`DILexicalBlock` was not); a
  change there rebuilds nearly every module.
- **Code that switches the builder into another function and back** (closures) must save and restore the builder's
  debug location as well as its block; otherwise the outer function carries on with the inner one's location, which
  the verifier rejects ("!dbg attachment points at wrong subprogram").
- **No `.debug_names`** (`DebugNameTableKind::None`): coroutine splitting gives `.resume`/`.cleanup` linkage names the
  index lists against entries that carry none, which `llvm-dwarfdump --verify` reports as mismatched names.
- **Verify the object (`out/.../llvm/spp.o`), not the executable:** GNU ld tail-merges `.debug_str`, so an offset can
  point into the middle of a string; valid, but `--verify` flags it (the same object linked with lld verifies clean).
- **Paths are relative to the project, with the compile directory `.`** (`DebugFileOf`, `LlvmCtx::DiRoot`). Absolute
  paths would put the build directory into every object, which fails `check-reproducible.sh`.
- **Full debug info with SafeStack is expensive in huge functions:** SafeStack re-anchors each moved variable's
  declaration on the unsafe stack pointer, so the back end tracks it instruction by instruction. One generated
  function with 31,066 declarations took 9.7 s and 3.5 GB to emit, against 0.9 s and 1.8 GB with line tables, and
  four at once ran WSL out of memory. An ordinary program pays little. `check-intrinsics.sh` uses `info = "lines"`
  (upstream-watch U29).
- **Use LLVM 23's `llvm-dwarfdump`** (`/usr/lib/llvm-23/bin/llvm-dwarfdump`): the `llvm-dwarfdump-14` on PATH cannot
  read the DWARF 5 name index forms LLVM 23 writes.
- A failed check's call (`sppc_check_failed`) is `nomerge` while there is debug info: merged calls keep the line of
  none of them.

The crash report does not read DWARF: the compiler derives a source-line table from it and links that in
([[source-line-table]]).

**Why:** the base the crash report (A2) and the debugger (part B) build on.
**How to apply:** new code that builds a function body with `ctx->Builder` either goes through `AttachSubprogram` or
emits into a function without one with no location; `tests/codegen/check-debug-info.sh` covers the result.
Related: [[spp-codegen-cross-module-and-inherited]].
