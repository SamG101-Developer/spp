---
name: runner-concurrency-and-cell-times
description: "20 concurrent jobs (free) until ~2026-11-13, then 40 (Pro via the student pack); a PR run asks ~48 lane jobs + ~30 others; per-cell times"
updated: 2026-11-11
---

The account's runner limit is 20 concurrent jobs, all OSes together (macOS 5 of them), until about 2026-11-13, when
Pro (GitHub student pack) raises it to 40. A PR run asks for far more: 8 lanes of one build and 5 test cells each
(Rel/Dev × 2 shards, plus the STL/extras cell), then changes, version, lint, cppcheck, benchmarks, docs, security,
analysis and cross-codegen (~23 emit jobs, gated behind the Linux lanes).

Cell times, run 42721403742 (minutes): gtest shards Linux GCC 17-27, Linux Clang 21-32, Linux ARM GCC/Clang 16-17,
Windows MSVC 24-46, Windows ARM MSVC 28-31; STL/extras 13-18 on Linux, 1-2 on Windows; builds 2-16. About 580
runner-minutes of gtest per run, half of it Dev. Linux Clang's shards take twice the ARM lanes' for the same tests.

**Per-test time (2026-11-11, run 42876363131, GTest Rel 1/2, 1734 tests, 4 at a time):** median Linux ARM Clang 2.9 s,
Linux Clang 5.5 s, Linux GCC 5.0 s, Windows MSVC 8.2 s. One compile on its own is about as fast on each (the STL
conformance step: 25-35 s on Linux x64, 28 s on ARM, 30 s on Windows), so the gap is in running four at once.
gtest-parallel starts one worker per CPU, 4 everywhere. The x64 runners' 4 vCPUs are most likely 2 cores with SMT,
while ARM's are 4 real cores, which matches x64 Linux at ~2x. Windows' extra ~1.5x is most likely per-process cost
(a process per test) on top of that; not measured. Not Defender: `exclude-defender-windows.sh` reports real-time
protection already off on the image. A lane's `gtest-workers` input (`_lane.yaml` → `_test.yaml` → `run-tests` →
`SPP_CI_GTEST_WORKERS` → `gtest.sh --workers`) sets the count; Windows MSVC on PRs tries `"2"` (2026-11-11). Half the
workers only pays if each test gets more than 2x faster (the shard then finishes sooner); between 1x and 2x it shows
contention but the shard is slower. Compare the median per test as well as the cell time.

**Why:** sharding and modes are chosen against this budget; with 20 slots the cells queue behind each other.

**How to apply:** if runs queue badly, the cheap cuts are Dev mode on Linux GCC and Windows MSVC only, and Rel-only
single-shard advisory lanes (clang-cl, Windows ARM): `modes:`/`shard-count:` inputs in `pr.yaml`, ~35-40% fewer
runner-minutes. See [[job-graph-layout]].
