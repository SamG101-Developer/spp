---
name: air-needs-wide-vectors
description: "The AIR passes act only on 256-bit vectors: set [target.floor] cpu (x86-64-v3, native); baseline x86-64 is left alone"
updated: 2026-11-09
---

Both AIR passes in `llvm_air` (`DeferOverflowChecksPass`, `ExactSumsPass`) ask LLVM's cost model
(`TargetTransformInfo`, per function) for the fixed vector width and do nothing below 256 bits
(`kAirMinVectorBits`). Baseline x86-64 is SSE2: two 64-bit lanes and no 64-bit arithmetic shift or compare, so a
vectorised check costs about what the branching one did (measured: 3.5% and 4.8% saved, and a 10% loss on short
loops). Built for AVX2 with `[target.floor] cpu = "x86-64-v3"` (or `"native"`), checked sums run twice as fast as C
and Rust (`checked_sum`: 23 ms against 42 and 44).

The CPU reaches LLVM twice: the target machine (`SelectedTargetMachine`, chosen by `SelectCpu` before it is built)
and every function's `target-cpu`/`target-features` (`ApplyTargetToFunctions`, before optimising; a function's own
features kept after the program's, so they win).

**Why:** a test or benchmark of AIR on a default build sees nothing happen, by design.
**How to apply:** exercise AIR with the cpu key (`check-air-integers.sh` builds for x86-64-v3 and skips without
AVX2). A future per-function `!target_feature` (an `arch.spp`) only has to add that function's `target-features`:
the gate is asked per function. Related: [[sroa-skips-sub-byte-coro-index]], [[variant-phis-hide-pointer-payloads]].
