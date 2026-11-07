---
name: benchmark-records-branch
description: "Benchmark records live on the orphan benchmarks branch, written only by post_merge's Publish Benchmarks job; _benchmarks.yaml stays read-only"
updated: 2026-11-02
---

`_benchmarks.yaml` has two jobs against the Linux GCC lane's `spp`, each compared with `compare.py` (report-only):
`Compile Time` (`benchmarks/run.py`) and `Run Time` (`benchmarks/runtime.py`, with Clang from `llvm.sh` at
`[pin.llvm-lib]` and rustc from `[pin.rust]`, installed by `install-rust-linux.sh` from verified component tarballs).
Neither writes. Each uploads a `benchmark-record-<kind>` artifact; `post_merge.yaml`'s `Publish Benchmarks` job, the
only one with `contents: write`, downloads them by pattern and adds them to the orphan `benchmarks` branch as
`records/<sha>/<kind>.json` and `latest/<kind>.json` (`.github/scripts/bench/publish-record.sh`), making the branch on
its first run. A PR reads its base commit's record of each kind, else `latest/` (`fetch-baseline.sh <kind>`).

**Why:** a called workflow's job may not ask for more permissions than the calling job grants, and that is checked when
the run starts, even for a job whose `if` would skip it. So the write cannot sit in `_benchmarks.yaml`, which PRs call
with `contents: read`; it is a job of its own in the caller that alone writes.

**How to apply:** never write to the branch by hand or from a PR. The benchmark jobs need the whole Linux GCC lane (a
called workflow's jobs cannot be needed one by one), gated on `compile` and on the lane's `artifact` output being set,
as Sonar's is. A failure in either job skips publishing for that commit. A ruleset protecting `benchmarks` must still
let `github-actions[bot]` push to it. Test the scripts offline with a bare repository and
`GIT_CONFIG_KEY_0=url.<bare>.insteadOf` for `https://github.com/<repo>.git`. `refresh-pins.sh` bumps `[pin.rust]` to the
latest stable; check its LLVM major still matches `[pin.llvm-lib]` (`rustc -vV`).
