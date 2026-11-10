---
name: runner-concurrency-and-cell-times
description: "20 concurrent jobs (free) until ~2026-11-13, then 40 (Pro via the student pack); a PR run asks ~48 lane jobs + ~30 others; per-cell times"
updated: 2026-11-10
---

The account's runner limit is 20 concurrent jobs, all OSes together (macOS 5 of them), until about 2026-11-13, when
Pro (GitHub student pack) raises it to 40. A PR run asks for far more: 8 lanes of one build and 5 test cells each
(Rel/Dev × 2 shards, plus the STL/extras cell), then changes, version, lint, cppcheck, benchmarks, docs, security,
analysis and cross-codegen (~23 emit jobs, gated behind the Linux lanes).

Cell times, run 42721403742 (minutes): gtest shards Linux GCC 17-27, Linux Clang 21-32, Linux ARM GCC/Clang 16-17,
Windows MSVC 24-46, Windows ARM MSVC 28-31; STL/extras 13-18 on Linux, 1-2 on Windows; builds 2-16. About 580
runner-minutes of gtest per run, half of it Dev. Linux Clang's shards take twice the ARM lanes' for the same tests
(not yet looked at).

**Why:** sharding and modes are chosen against this budget; with 20 slots the cells queue behind each other.

**How to apply:** if runs queue badly, the cheap cuts are Dev mode on Linux GCC and Windows MSVC only, and Rel-only
single-shard advisory lanes (clang-cl, Windows ARM): `modes:`/`shard-count:` inputs in `pr.yaml`, ~35-40% fewer
runner-minutes. See [[job-graph-layout]].
