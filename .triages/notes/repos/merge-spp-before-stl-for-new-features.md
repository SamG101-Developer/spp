---
name: merge-spp-before-stl-for-new-features
description: The gtest fixture and CI compile SPP-STL master, so std code relying on a new compiler feature merges after the spp change, never before; the benchmark jobs pin benchmarks/std.rev instead
updated: 2026-11-09
---

spp's gtest fixture (`SppBootstrap.Fixture`) and its CI compile SPP-STL `master` from GitHub. So std code that relies
on a compiler change that is not merged yet (pack-design phases 3-7: `drop(..xs)`, `&..self` into `..tail: &Ts`,
`std::format`) breaks every spp build on `master` once it is pushed, as the merged compiler rejects it. The other
order is safe when the new compiler still compiles the old std. Checked 2026-11-04: the pack compiler passes all 554
tests of committed std, and 593 of 594 of the std with `format.spp` (the one failure is a known red test).

**The benchmark jobs are the exception:** `benchmarks/runtime.py` and `run.py` build against the std commit pinned in
`benchmarks/std.rev`, not master. A compiler change that stops compiling an older std (2026-11-09: `Ptr` lowered to a
bare `ptr`, dropping the old `addr` field's layout) breaks both CI benchmark jobs, with an ICE in the pinned std's
`pointer.spp` ("Invalid indices for GEP pointer type"), until `std.rev` is bumped to a std the compiler accepts. Bump it
in the same spp change.

**Why:** the pack redesign's std half (variadic `drop`, the formatter) needs phases 3-7 of the compiler.

**How to apply:**
- Merge the spp change, then push SPP-STL.
- Before pushing, test the working-tree std against the new compiler. Copy `../spp-stl` with `cp -a`, since rsync is not
  installed, into a scratch project's `vcs/std`, with `origin` pointed at the local repo so the pull is a no-op. Then run
  `spp test -l std`.
- After the push, `git -C tests/test_outputs/vcs/std checkout .` clears any hand-copied file, so the fixture can pull.

Related: [[spp-project-std-and-prelude]].
