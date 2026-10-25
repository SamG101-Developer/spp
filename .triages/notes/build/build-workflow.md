---
name: build-workflow
description: "Building, the fast loop, and running and reading the gtest suite without being fooled by stale or killed runs"
updated: 2026-10-25
---

Default to the release build; use debug only when debug info is needed (its LLVM verifier output, "Verification
failure in function ...", is the richest signal for bad IR), and the asan build (`SPP_SANITIZER=address`, see
[[asan-build-for-llvm-crashes]]) for crash traces, since gdb cannot unwind through the prebuilt LLVM.

**Fast loop first.** Iterate with one-file probes (see [[spp-probe-harness]]), seconds per case. `spp build -m dev`
builds the current directory (there is no `--dir`), compiling and linking; only optimisation/LTO are `-m rel` only.
`spp test -g <group>` runs the s++ suites. Reserve the gtest suite for confirming a change before handing it over.

**The suite:** `bash tests/compiler/run.sh` (gtest-parallel, ~30 min for ~3180 tests). Always `cmake --build ... --parallel` in the
same command first: a run started without it uses whatever `spp_tests` was last linked and reports the old test
sources (three fixed tests once read as a fresh regression).

- **Targeted changes get a targeted run, not the full suite.** Pick the suites the changes touch from
  `spp_tests --gtest_list_tests` (generics/errors/tuples for a solver change, calls/overloads/parser for a cast change),
  run them with `run.sh --gtest_filter=...`, and say which areas were covered and which were not. The full suite only
  when asked, or for a change whose reach cannot be bounded (a harness-wide check). Disabled tests are listed but do not
  run, so the run count is below the listed count.
- **Read the result by counting files in `tests/gtest-parallel-logs/failed/`**, not by parsing the log text. Names are
  `spp_tests-<Suite>_<test>-<n>.log` (`.` becomes `_`).
- **Wipe `tests/gtest-parallel-logs/` first if an earlier run was killed**: leftovers make the next run die in
  `logger.move_to('passed', ...)` with a `FileNotFoundError` after every test has run, and the counts are the old
  run's residue.
- **A run killed part-way leaves `failed/` empty** (logs are filed only at the end), so "0 failed" from it means
  nothing; the truth is in the log's `returned with exit code` / `killed by signal` lines. Under memory pressure,
  `(ulimit -v 4000000; bash tests/compiler/run.sh -w 6)` alone is the safer shape.
- **Some tests are red on HEAD itself.** Attribute a failure by re-running it serially against a build with the change
  stashed before blaming the change.
- **Never overlap two suite runs**: they share `tests/gtest-parallel-logs`. When chaining background jobs, wait on a
  marker only that job's own phase writes, or on the job's completion.

**Run the suite binary from `<build-dir>/tests/`, never the build dir itself.** `build_temp_project` resolves the
scratch project as `<cwd>/../../tests/test_outputs`, so the cwd silently picks which one is used. The wrong one fails
*invertedly*: every compile aborts on `'ffi/sppc' has no 'libsppc.so'`, so every should-fail test fails and every
should-pass test trivially passes (once read as an 880-test regression). Tells: `has no 'libsppc.so'` in the log, and
per-test times of 0 ms where a real compile takes 1-2 s. A cwd resolving to a fresh directory is worse: `test_boot`
clones std from upstream GitHub, which may predate local language changes.

**Stopping a build or suite from a script: kill by PID, never `pkill -f <pattern>`.** If the pattern appears in the
job's own command line (e.g. the rebuild it starts next), it kills itself (exit 144). `pgrep -f` in the same command as
the `kill` has the same trap. List PIDs with `pgrep -a` in one call, kill them in the next. A killed `cmake` can leave
its `ninja` running.
