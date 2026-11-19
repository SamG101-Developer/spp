---
name: compile-bench-long-function-instructions-vary
description: "long-function's cachegrind instruction count swings 5-9% between runs of one binary, almost all in LLVM's DbgValueHistoryMap::trimLocationRanges; judge it by counters or a cg_diff"
updated: 2026-11-19
---

`benchmarks/run.py --cachegrind` counts instructions once per workload. For `long-function` (dev) that count is not
reproducible:
- **Same binaries, two runs on 2026-11-19, std `741c0f71`:** HEAD measured 166.9 billion then 181.6 billion; the
  working tree 173.1 billion then 167.9 billion.
- **The comparison flipped:** one run reported "+3.71% regressed", the next "-7.51% improved".
- **`cg_diff` of the second pair:** 99.6% of the difference (13.59 of 13.64 billion) was in
  `llvm::DbgValueHistoryMap::trimLocationRanges`, debug-info range trimming during code generation. It is superlinear
  on the workload's one 1,500-statement function.
- **The next entries down are `SmallPtrSet` lookups**, which suggests the cost follows memory addresses (ASLR), not the
  compiler's work. The compiler's own functions moved by under 0.02% each.

The other workloads (`generic-depth`, `hello`, `many-modules`) stayed within 0.03%.

**Why:** a regression read off `long-function`'s instruction count alone can be noise of this size, in either direction.

**How to apply:**
- Judge compile-time work by the counters, which are exact and per phase (`compare.py` names the phase).
- Before calling a `long-function` instruction change real, keep the profiles (`run.py --work <dir>`; the output is
  `projects/long-function/bench/cachegrind-dev.out`) and `cg_diff` them. If the difference is
  `trimLocationRanges`, it is this.
- Running cachegrind with ASLR off (`setarch -R`) might make the count stable. That's untested.

Related: [[benchmark-records-branch]].
