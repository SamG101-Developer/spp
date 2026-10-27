---
name: ci-runner-core-topology
description: "ARM CI is faster than x86 because ubuntu-24.04 gives 2 physical cores and ubuntu-24.04-arm gives 4; not a config bug"
updated: 2026-09-09
---

GitHub's `ubuntu-24.04` and `ubuntu-24.04-arm` both advertise 4 vCPU, but they are not
comparable. Measured 2026-09-09 by the CPU topology block in
`.github/scripts/test/gtest.sh`:

- `ubuntu-24.04-arm`: Neoverse-N2, 1 thread/core, 4 cores/socket -> **4 physical cores**.
- `ubuntu-24.04`: AMD EPYC, 2 threads/core, 2 cores/socket -> **2 physical cores**.

The x86 pool is also heterogeneous: two jobs in the same run landed on an EPYC 9V74 (Genoa)
and an EPYC 7763 (Milan). Normalised for SMT the x86 Genoa core is ~1.25x an N2 core, so x86
loses on core count and then varies again on which silicon it draws.

The test sweep is ~2100 independent process spawns at `cpu_count()` workers, so it scales with
cores: ARM ~18 min, x86 23-34 min. The build does *not* show the gap (243 `.ixx` modules
serialise on the module graph, so it measures single-thread speed and comes out at parity).
A build-vs-test split like that is the signature to look for.

**Why:** this looks exactly like a misconfigured x86 job and it is not one. Do not go hunting
for a workflow bug; do not lower `workers` to the physical core count either - 4 workers on 2
SMT cores beats 2 workers on 2 cores by ~25%, so the `cpu_count()` default is already right.

**How to apply:** treat x86 CI timings as having a wide floor-to-ceiling spread by design, and
never use an ARM time as the expected x86 time. To actually cut the critical path, shard the
suite across jobs or move the blocking gate to ARM - not by retuning worker counts.

Related: [[spp-test-suite-stage-scope]], [[gtest-parallel-crash-reporting]],
[[spp-test-group-filter-bugs]].
