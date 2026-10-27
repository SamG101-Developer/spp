---
name: ffi-struct-params-c-abi
description: "An !ffi function's struct parameter takes the target's C form (byval on ppc32/m68k, by pointer or integer on s390x); one-field structs count as scalars"
updated: 2026-10-27
---

LLVM passes a struct argument as its fields, which matches the C calling convention only where the target passes a
small struct in registers. `CAbiParamsOf` (`../sources/spp/codegen/llvm_fn.cpp`) gives an `!ffi` declaration the C
form instead, and `CAbiAdaptArgs` makes each call fit it through a stack copy: 32-bit PowerPC and m68k take a C struct
`byval`; s390x one of 1, 2, 4 or 8 bytes as an integer that size, any other as a pointer to a copy; every other target
as it is. A C struct is a struct of two or more fields: one of a single field (`NonNull`, a wrapped pointer or
integer) stands for that field's C scalar. Returned structs ("sret") are not lowered; sppc returns none.

**Why:** with the fields passed in registers, sppc read `sppc_closure` (`CClosure` in SPP-STL's stub) as a pointer to
one, so every thread and future crashed in `sppc_pthread_create` on ppc32 and s390x (2026-10-27; 531/554 on ppc32).
**How to apply:** the rules came from `clang --target=<t> -S -emit-llvm` on a C prototype taking the struct; check a
new target, or a new struct shape crossing into sppc (a struct over 16 bytes, floats, a returned struct), the same
way before relying on "as it is". `tests/codegen/check-c-abi.sh` checks the three rules on `sppc_pthread_create`'s
declaration; it needs an all-targets spp (it skips otherwise), so CI runs it in the cross-codegen lane's ppc32 entry.
