---
name: stack-protector-heuristics
description: "Stack canaries are on for every function; LLVM's ssp heuristics are C-shaped and neither fits s++"
---

`ApplyStackProtector` in `llvm_harden_memory.cpp` stamps `sspstrong` on every function with a body, after optimisation and
before object emission - one place, so coverage does not depend on which of the five `Function::Create` sites a
function came through, and there is no caller/callee attribute propagation to reason about because inlining is done.

**Why:** measured on the sample project + std, `.text` unoptimised / optimised, functions protected:

| level | dev .text | protected | rel .text | protected |
|---|---|---|---|---|
| off | 21388 | 0/167 | 5078 | 0/34 |
| basic (`ssp`) | 21529 | **2**/168 | 5251 | 3/35 |
| strong | 25257 (+18%) | **169**/168 | 5283 (+4%) | 5/35 |
| req | 28113 (+31%) | 304/168 | 5531 | 17/35 |

The basic heuristic wants a C character buffer and finds almost nothing; `sspstrong` protects any frame with an
address-taken local, and second-class borrows take the address of nearly every local, so it degenerates to "all". In C
`sspstrong` is targeted; here it is not.

**How to apply:** the canary is not covering what it covers in C - bounds checks, no pointer arithmetic and no
dangling borrows already remove stack smashing from safe code. It is there for the FFI boundary and the raw-pointer
core of `std`. If the cost ever matters, the fix is a real "calls FFI or handles a raw pointer" analysis rather than
dropping to `ssp`, which protects nothing here. Verify with `objdump | grep 'fs:0x28'` for the prologue and
`grep -B6 __stack_chk_fail` for the epilogue compare.

**Validating it fires:** `tests/codegen/check-stack-protector.sh` smashes a frame from foreign code and asserts the canary
catches it, in both profiles, plus an in-bounds control. It has to link and run, which the c++ suite never does. The
smash route is the only one left in the language: `Arr::ptr` is package-private so a project cannot hand out a raw
pointer to a local, but an `!ffi` function can take a `&mut U8` and a borrow is a pointer at the abi. Confirm the check
is not a false pass by making `ApplyStackProtector` skip everything - the overflow cases must then fail with 139 and no
message. The script names the project with `--dir`: `spp` takes the project from that option and otherwise from the sample
project beside the binary, never from the working directory (`main.cpp`).
