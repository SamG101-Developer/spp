---
name: ffi-struct-params-c-abi
description: "!ffi declarations take the target's C form: structs byval/by pointer (ppc32, m68k, s390x), small integers signext/zeroext; one-field structs are scalars"
updated: 2026-10-27
---

LLVM passes a value as it is: a struct as its fields, an integer with whatever is above it in its register. Neither
always matches the C calling convention, so `!ffi` declarations take the C form (`../sources/spp/codegen/llvm_fn.cpp`,
from `GenerateLlvmDeclaration`), and calls fit it through `CAbiAdaptArgs`:
- **Structs** (`CAbiParamsOf`): 32-bit PowerPC and m68k take a C struct `byval`; s390x one of 1, 2, 4 or 8 bytes as
  an integer that size, any other as a pointer to a copy; every other target as it is. A C struct is a struct of two
  or more fields: one of a single field (`NonNull`, a wrapped pointer or integer) stands for that field's C scalar.
- **Integers** (`CAbiExtendOf`, on parameters and the return): every target extends `char`, `short` and `bool`;
  ppc64(le), s390x and sparc64 extend `int` by its signedness, riscv64, loongarch64 and MIPS sign-extend it either
  way. Signedness is the S++ type's own `signed` comp argument (`type_predicates::IsTypeSignedInteger`).
- Returned structs ("sret") are not lowered; sppc returns none.

**Why:** with the fields passed in registers, sppc read `sppc_closure` (`CClosure` in SPP-STL's stub) as a pointer, so
every thread crashed on ppc32 and s390x; without the extension, s390x's sppc read a clock id or socket fd with
garbage above it, so clocks and sockets failed depending on what ran before (2026-10-27).
**How to apply:** the rules came from `clang --target=<t> -S -emit-llvm` on C prototypes; check a new target, or a new
shape crossing into sppc (a struct over 16 bytes, floats, a returned struct, a 64-bit integer on MIPS), the same way.
`tests/codegen/check-c-abi.sh` checks the rules on sppc declarations for ppc32, s390x, riscv64 and x86_64; it needs
an all-targets spp (it skips otherwise), so CI runs it in the cross-codegen lane's ppc32 entry.
