---
name: spp-probe-harness
description: "Fast way to fuzz the compiler with many small programs - a scratch copy of the test fixture, and the exit-code trap that hid segfaults"
updated: 2026-10-20
---

To probe many one-file programs without touching `./project` or the gtest binary: copy `tests/test_outputs/{spp.toml,
ffi,vcs}` into a scratch dir, write `src/main.spp` (prepend `fun main() -> Void { }` like the harness does), and run
`spp build -m dev` from inside the scratch dir (`--dir` was removed 2026-10-12) (~3 s each, compiles + links std; the run re-fetches std from GitHub).

**Capture the exit code from a file-redirected run, never through a pipe.** `$(... | tr ...)` loses the compiler's
status, and a segfault printed nothing to classify - so crashes were reported as "pass" until re-run
(2026-09-27). Treat rc >= 124 as a crash; also grep for LLVM verifier text ("Invalid module"), which the dev build
prints without an `E<n>` code.

Get crash traces from the asan build (`<debug-asan build dir>`, see [[asan-build-for-llvm-crashes]]); for an
uncaught exception, gdb `break __cxa_bad_cast` / `catch throw` works on it. Run gtest-parallel with `--output_dir`
pointing at a scratch dir so `tests/gtest-parallel-logs` (the maintainer's baseline) is not overwritten. See [[build-workflow]].

**Never run two probe batches against the same scratch project at once.** `run.sh` writes `$P/src/main.spp` and reads
`$P/raw.log`, so a concurrent probe overwrites a running batch's source, and each one reports the other's result (seen
2026-09-28 as an `exit exit 1` error under an unrelated test). Give each concurrent run its own `P=`: `p` and `p2` work, but `p3`'s vcs/std has stale local edits, so its fetch
conflicts and every probe reports OTHERERR. Also never rebuild `spp` while a batch runs.

**The scratchpad is in /tmp and does not survive a reboot** (lost 2026-10-01 after a BSOD: run.sh, the fixture and every
probe set), nor a session restart (lost again 2026-10-07: irdiff.sh, ir-base and p2 had to be rebuilt; take a new
IR baseline before changing anything), nor even a running session crossing midnight (wiped 2026-10-16 ~00:27 mid-task:
probes, run_fwd.sh, irnames.py and the ir-pD baseline; only files written after it survived). Rebuild a baseline from
a pre-change backup of the edited file (build, `spp test --all-libs`, copy `project/out/.../rel/llvm`), so back up the
edited file before each change. Recreate: `mkdir -p $SP/p2/src $SP/out`, copy `tests/test_outputs/{spp.toml,ffi,vcs}` into `$SP/p2`, rewrite
`run.sh` (loop: prepend main, file-redirected run, classify rc>=124 crash / `E<n>` fail / else pass, print vs the
`# EXPECT:` header). Probes worth keeping across sessions belong in `tests/` as real tests.
Wiped again mid-session 2026-10-20 (backups and the std project gone): the std run project is a copy of
`./project`'s `spp.toml ffi src tst vcs` into `$SP/irp`, then `spp test --all-libs` from inside it.
