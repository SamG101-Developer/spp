---
name: runner-concurrency-and-cell-times
description: "20 concurrent jobs (free) until ~2026-11-13, then 40 (Pro via the student pack); a PR run asks ~48 lane jobs + ~30 others; per-cell times"
updated: 2026-11-13
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
gtest-parallel starts one worker per CPU, 4 everywhere. The x64 runners' 4 vCPUs are most likely 2 cores with SMT, while
ARM's are 4 real cores, which matches x64 Linux at ~2x. Windows' extra ~1.5x is not per-process cost: measured
2026-11-13 from gtest-parallel's per-test times, a test that does nearly nothing takes 16 ms on Windows against 6 ms on
Linux, ~10 ms a process, ~20 s a 1,747-test shard. The time is in each test's compile (median 5.2 s Windows, 3.4 s
Linux; most compile a program with the std). Not Defender: `exclude-defender-windows.sh` reports real-time protection
already off on the image. Windows MSVC on PRs tried 2 workers (2026-11-11, a `gtest-workers` lane input): a wash
(2026-11-13 run: 5.2 s median a test against 8.2 s with 4, 1.6x, so ~60 min a shard either way; the runner is
compute-bound), so the input was removed again (2026-11-13) and every lane runs one worker per CPU. Half the workers
only pays if each test gets more than 2x faster (the shard then finishes sooner); between 1x and 2x it shows contention
but the shard is slower. Compare the median per test as well as the cell time.

**Shard counts (2026-11-13):** the Windows lanes (MSVC, clang-cl, ARM MSVC; the release Windows lane) run 4 gtest
shards, Linux and macOS 2. macOS stays at 2 because of its 5-slot cap: Rel/Dev at 2 shards plus the extras cell is
exactly 5 cells. At 4 shards the 9 cells take two rounds of half-length shards, which ends no sooner.

**Why:** sharding and modes are chosen against this budget; with 20 slots the cells queue behind each other.

**How to apply:** if runs queue badly, the cheap cuts are Dev mode on Linux GCC and Windows MSVC only, and Rel-only
single-shard advisory lanes (clang-cl, Windows ARM): `modes:`/`shard-count:` inputs in `pr.yaml`, ~35-40% fewer
runner-minutes. See [[job-graph-layout]].
