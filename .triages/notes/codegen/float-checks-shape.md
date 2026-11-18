---
name: float-checks-shape
description: "Float checks: one hot uno (r - r),0 branch to a cold classifier; AIR defers only the hot flag (kind 0x23) and drops a +-* check its checked user covers"
updated: 2026-11-17
---

`EmitFloatCheck` (`llvm_fn_impls.cpp`) checks a float result with one hot branch, `uno (r - r), 0 ? classify : ok`
(`r - r` is a NaN exactly when `r` is not finite). The cold `float.classify` block names the kind: `uno r, r` is
invalid, else division by zero where the `by_zero` callback's test holds (a zero divisor, a pole such as `log(0)`;
built only there), else overflow. Any non-finite result traps, operands regardless (values made non-finite on purpose
go through `*_ieee`). Each abort is `sppc_check_failed(0x20..0x23, op, width)` with constant arguments: `op` is the
`FloatOp` number (sppc names it), as the crash path cannot print a float.

**Only `uno` compares, and finiteness as `x - x`.** SSE2 compares `eq/lt/le/ord` and their negations in one
instruction; `one` and `ueq` take two, and `|x| == inf` became an integer compare of 64-bit lanes (no `pcmpeqq`).
`|x| olt inf` and `|x| uge inf` do not help: InstCombine turns them into `one` and `ueq`. `x - x` is not folded
without `nnan`.

**The deferral never reads the classifier.** LLVM simplifies the classifier with what the guard told it ("r is not
finite"), so its flags are wrong anywhere but under the guard: a first guarded version, whose pass reused them per
element, trapped on every finite element. `llvm_pass_air` now ORs only the hot flag per element and, at the exit,
reports `FloatNotFinite` (0x23, "a NaN or infinite") through a copy of one of the classifier's aborts with the kind
swapped; the classifier (which reads `r` from inside the loop) is deleted. `FloatAbortBehind` recognises the cold
block however SimplifyCFG has reshaped it: side-effect-free blocks that lead only to float aborts (`IsAbortPath`,
`llvm_pass_loop_versioning.cpp`). Every pass that asks "is this exit only an abort" must use `IsAbortPath`, not
`IsAbortBlock`: the bounds pass did not, so float-checked loops with an index check stopped being versioned (and so
deferred and vectorised: 2.9x on `checked_float_scale`), and `VersionLoop` copies a whole abort path, phis and all,
when it reads the loop.

**Chain elision** (`ElideChainedFloatChecksPass`, first of the AIR passes): the check of an `fadd`/`fsub`/`fmul` is
dropped when it has at least one real user and every one is a checked `+ - *` or the dividend of a checked `/` `%`
(`PassesNonFinite`: an infinite divisor gives a finite quotient). The later check stops it, naming the later
operation. A division keeps its own check (division by zero stays exact), and an unused result keeps its check (an
empty "every user" would drop a whole chain: found by `check-float-checks.sh`). `sqrt` passes one on too. A branch
inside a classifier is never a check: LLVM rewrites its tests over the operands ("sqrt(t) is a NaN" as `t ult 0`), so
it looks like a check of `t`, and dropping it with `t`'s made `acosh(0.5)` report an overflow.

**Deferred only where the loop can vectorise** (`CanVectoriseFloats`): not under 8 known rounds, no float phi, no
store to a loop-invariant address. Elsewhere the flags cost more than the predicted branches (`nbody` 2.3x deferred,
1.7x not).

**Maths functions** (F7): `SimpleCheckedFloatMath` calls the libm intrinsic and checks its result the same way; an
infinity at a pole is a division by zero. `FloatOp` 8-24 name the functions (sppc's `_sppc_float_op_name`); a new one
needs both sides, as does a new check kind.

Float deferral needs no 256-bit vectors (float compares vectorise on SSE2); a loop with float checks defers its
integer ones too. Measured (2026-11-17, `a[i] = a[i] * b[i] + 1`, 1024 doubles): 2.6x unchecked with two branches per
operation, 1.26-1.29x with one hot test and the multiply's check elided; a scalar recurrence within noise.

**Why:** the pass relies on the shape; a change to the emitted check must keep the abort arguments constant and the
hot test a direct `flag ? cold : next` branch, or loops silently stop being deferred (and vectorising).

**How to apply:** `tests/codegen/check-float-checks.sh` asserts a loop is deferred (`air.`) and vectorised, and the
reported kinds per mode; run it after touching either side (`STD_BRANCH`, `SPPC_LIB` while std or sppc are
unpushed). Related: [[checked-results-are-flagged]], [[spp-single-abort-path]].
