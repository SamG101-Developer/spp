---
name: ci-runner-core-topology
description: "ARM CI is faster than x86 because ubuntu-24.04 gives 2 physical cores and ubuntu-24.04-arm gives 4; not a config bug"
updated: 2026-11-08
---

GitHub's `ubuntu-24.04` and `ubuntu-24.04-arm` both advertise 4 vCPU, but they are not
comparable. Printed by the CPU topology line in `.github/scripts/test/gtest.sh`:

- `ubuntu-24.04-arm`: Neoverse-N2, 1 thread/core -> **4 physical cores**, same silicon every job.
- `ubuntu-24.04`: 2 threads/core -> **2 physical cores**, on a pool that varies per job: EPYC
  7763, 9V74, 9V45, Xeon 8370C, Xeon 857x seen.
- `macos-15` (no lane since 2026-11-08; macos-26 not yet probed): M1 (Virtual), **3 workers**. `windows-2025`: 4 logical, topology not probed.

The test sweep (3310 tests on 2026-11-02) is independent process spawns at `cpu_count()` workers,
so it scales with cores. Whole-sweep minutes over three PR runs, 2026-11-01/02: ARM 30-35, x86
38-59 by silicon, Windows 49-84, macOS 60-80. The same binary swings on draw alone: GCC shards of
one mode took 25 min on the Xeon and 29 on the 7763 in one run. GCC and clang match on the same
silicon, so a lane difference on x86 is the draw, not the compiler. Add shards' times only for job
minutes; the lane's wall clock is the slowest shard.

The build does *not* show the ARM gap (243 `.ixx` modules serialise on the module graph, so it
measures single-thread speed and comes out at parity; 2026-09-09). A build-vs-test split like that
is the signature to look for.

**Why:** this looks exactly like a misconfigured x86 job and it is not one. Do not go hunting
for a workflow bug; do not lower `workers` to the physical core count either - 4 workers on 2
SMT cores beats 2 workers on 2 cores by ~25%, so the `cpu_count()` default is already right.

**How to apply:** treat x86 CI timings as having a wide floor-to-ceiling spread by design, and
never use an ARM time as the expected x86 time. To actually cut the critical path, shard the
suite across jobs or move the blocking gate to ARM - not by retuning worker counts. GitHub's
larger runners need an org on Team or Enterprise (the repo is on a personal account, 2026-11-02)
and are billed even for public repos. Per-test time is nearly constant (72% of suite time is in
tests within 2.4-3.4s on ARM), so most of it is repeated per-test work, not the test's own code.

Related: [[spp-test-suite-stage-scope]], [[gtest-parallel-crash-reporting]],
[[spp-test-group-filter-bugs]].
