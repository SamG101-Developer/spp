---
name: checked-results-are-flagged
description: "A checked +/-/* returns the plain op flagged nuw/nsw, not the intrinsic's value: LLVM's analyses can read it; passes moving a check must drop the flags"
updated: 2026-11-09
---

`EmitCheckedArith` (`llvm_fn_impls.cpp`) emits `*.with.overflow` for the check, branches on its overflow bit, and
returns the plain operation flagged as not wrapping (`add nuw` for unsigned, `add nsw` for signed; likewise `sub`,
`mul`) as the result. The flag holds wherever the result is used, as that is past the check; the op is poison if it
wraps, but nothing reads it there. Only the intrinsic's bit is used; the backend fuses it back with the op.

Until 2026-11-09 the result was the intrinsic's value (`extractvalue 0`), which LLVM's analyses cannot see through:
`(lo + 16) - lo` did not fold to 16, a loop bound built with checked arithmetic had no trip count, and checks the
arithmetic already proved stayed. `ast/slices` dropped from 10.5 to 4.0 billion instructions (-62%) on a default
build with this alone.

At the end of the optimiser, `FuseCheckedOpsPass` (`llvm_air`) puts each op and its check back into one
`*.with.overflow` for the backend, which only fuses them ("add", "jo") in that form: InstCombine turns a check whose
value is unused into a comparison and moves the op away, and without the re-fuse every checked op cost an extra
`cmp` (or a second multiply). It matches tests by value, not shape (truth set against overflow set for constants).

**Why:** the no-wrap fact is exactly what the check guarantees, and it feeds every analysis (InstCombine, SCEV trip
counts, range analysis, induction variables). Instruction counts can rise where LLVM then picks other forms (sieve
+43%, collatz +34%) while wall time falls (sieve -15%): judge by wall time.

**How to apply:** a pass that finds a checked op's result looks for the flagged op (`FlaggedResultsOf` in `llvm_air`:
same opcode and operands, `nsw` if signed else `nuw`), not only `extractvalue 0`. A pass that moves or removes the
check (`DeferOverflowChecksPass`) must `dropPoisonGeneratingFlags()` on those results: past a moved check a wrapped
result is used, and must be the wrapped value, not poison. The std's `*_overflow` methods keep the intrinsic's
value (they report the wrapped value with its flag). Related: [[air-needs-wide-vectors]].
