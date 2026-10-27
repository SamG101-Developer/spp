---
name: msvc-ice-machinememoperand
description: "MSVC 14.51 ICE (C1001, symbols.cpp:5758) instantiating PointerUnion from MachineMemOperand.h in importers of llvm.ixx"
---

MSVC 14.51.36231 ICEs (C1001, `symbols.cpp` line 5758) at `llvm/ADT/PointerUnion.h(355)` instantiating
`PointerUnion<const Value*, const PseudoSourceValue*>` (from `llvm/CodeGen/MachineMemOperand.h`) in any module that
does `import llvm;` (seen in `llvm_ctx.ixx`, `llvm_fn_impls.ixx`). Of `headers/wrappers/llvm.ixx`'s GMF includes, only
`llvm/CodeGen/MachineFunction.h` pulls MachineMemOperand.h. It and `TargetSubtargetInfo.h` were unused, so both were
removed with their `using` exports on 2026-10-25.

**Why:** a standing Windows build blocker since at least 2026-10-23.
**How to apply:** don't add CodeGen headers (MachineFunction, MachineInstr, ...) to llvm.ixx's GMF; if codegen-level
LLVM types are needed, use them from a plain TU instead. If the ICE recurs, find which include pulls
MachineMemOperand.h (`g++ -H` per include). See [[third-party-module-wrappers]], [[llvm-module-wrapper-exports]].
