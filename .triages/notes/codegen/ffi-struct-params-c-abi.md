---
name: ffi-struct-params-c-abi
description: "!ffi declarations take the target's C form: structs byval/by pointer (ppc32, m68k, s390x), small integers signext/zeroext; one-field structs are scalars, in and out"
updated: 2026-11-10
---

LLVM passes a value as it is: a struct as its fields, an integer with whatever is above it in its register. Neither
always matches the C calling convention, so `!ffi` declarations take the C form (`CAbiImpl` in
`../sources/spp/codegen/llvm_abi.cpp`, see [[abi-conventions-table]]), and calls fit it through `AdaptArgs`:
- **Structs** (`ParamsOf`): 32-bit PowerPC and m68k take a C struct `byval`; s390x one of 1, 2, 4 or 8 bytes as
  an integer that size, any other as a pointer to a copy; every other target as it is. A C struct is a struct of two
  or more fields: one of a single field (`NonNull`, a wrapped pointer or integer) stands for that field's C scalar.
- **Integers** (`CExtendOf`, on parameters and the return): every target extends `char`, `short` and `bool`;
  ppc64(le), s390x and sparc64 extend `int` by its signedness, riscv64, loongarch64 and MIPS sign-extend it either
  way. Signedness is the S++ type's own `signed` comp argument (`type_predicates::IsTypeSignedInteger`).
- **One-field structs** (a wrapped pointer or integer, however deeply) are declared as that scalar on every target,
  parameters (`AbiParam::Kind::Unwrap`, extracted at the call) and returns (`ReturnOf`, wrapped back up by
  `AdaptReturn` at the call). It is the C form, and what lets LLVM see a pointer as a pointer (O14:
  `sppc_malloc` returning `ptr` is what makes it an allocator, [[runtime-allocator-attrs]]).
- Returned structs of two or more fields ("sret") are not lowered; sppc returns none.

**Why:** with the fields passed in registers, sppc read `sppc_closure` (`CClosure` in SPP-STL's stub) as a pointer, so
every thread crashed on ppc32 and s390x; without the extension, s390x's sppc read a clock id or socket fd with
garbage above it, so clocks and sockets failed depending on what ran before (2026-10-27).
**How to apply:** the rules came from `clang --target=<t> -S -emit-llvm` on C prototypes; check a new target, or a new
shape crossing into sppc (a struct over 16 bytes, floats, a returned struct, a 64-bit integer on MIPS), the same way.
`tests/codegen/check-c-abi.sh` checks the rules on sppc declarations for ppc32, s390x, riscv64 and x86_64; it needs
an all-targets spp (it skips otherwise), so CI runs it in the cross-codegen lane's ppc32 entry.
