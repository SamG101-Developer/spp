---
name: gtest-parallel-crash-reporting
description: "gtest-parallel reports a crashed test as \"killed by signal SIGSEGV\", not \"returned with exit code\"; grepping only the latter reports zero segfaults when there are eleven"
---

A test that segfaults is reported by gtest-parallel as `[n/N] Suite.test killed by signal SIGSEGV (ms)`, and it gets **no** `returned with exit code` line. Its per-test log under `tests/gtest-parallel-logs/failed/` ends right after `[ RUN      ]` with no `[  FAILED  ]` marker, so a scan that looks for gtest's own failure text misses it too.

**Why:** on 2026-09-07 I scanned a full run for `returned with exit code`, found only `1`s, and told the maintainer there were no segfaults. There were eleven. The cheap cross-check that would have caught it: the count of files in `failed/` (68) did not match the number of result lines (58).

**How to apply:** count crashes with `grep -c 'killed by signal' <run.log>`, and treat `ls tests/gtest-parallel-logs/failed | wc -l` versus the summary's own count as a consistency check. To attribute a crash, run the one test under gdb - `gdb -q -batch -ex run --gtest_filter=... -ex "bt 3" <spp_tests>` gives usable frame names even from the release build. See [[build-workflow]], [[spp-test-group-filter-bugs]].
