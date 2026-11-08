---
name: windows-defender-already-off-x64
description: windows-2025 x64 hosted runners ship with Defender real-time scanning off; windows-11-arm has it on
updated: 2026-11-08
---

The `defender:` line from `.github/scripts/setup/exclude-defender-windows.sh` reads `real-time protection False` on
the windows-2025 x64 test shards and `True` on windows-11-arm (run 42498915706). Defender exclusions therefore cannot
speed up the x64 Windows lane. That lane's gtest shards take ~44 min against ~26 min on windows-11-arm and ~30 min on
Linux, with the same 1731 tests per shard and 4 workers: x64 hosted runners have 4 vCPU on 2 SMT cores, ARM has 4 real
cores (see [[ci-runner-core-topology]]).

**Why:** Defender-exclusion guides (e.g. WarpBuild's) and runner-images#14096 are the usual advice for slow Windows CI.
Neither addresses this lane: Defender is already off, and #14096 is a one-off cold-start delay on the first compile.

**How to apply:** for Windows x64 test time, look at shard count, Dev Drive (ReFS), or per-test process and link cost,
not Defender. Check the `defender:` log line again before you rely on this. Images change.
