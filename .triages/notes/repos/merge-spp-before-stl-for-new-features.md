---
name: merge-spp-before-stl-for-new-features
description: The gtest fixture and CI compile SPP-STL master, so std code relying on a new compiler feature merges after the spp change, never before
updated: 2026-11-04
---

spp's gtest fixture (`SppBootstrap.Fixture`) and its CI compile SPP-STL `master` from GitHub. So std code that relies
on a compiler change that is not merged yet (pack-design phases 3-7: `drop(..xs)`, `&..self` into `..tail: &Ts`,
`std::format`) breaks every spp build on `master` once it is pushed, as the merged compiler rejects it. The other
order is safe when the new compiler still compiles the old std. Checked 2026-11-04: the pack compiler passes all 554
tests of committed std, and 593 of 594 of the std with `format.spp` (the one failure is a known red test).

**Why:** the pack redesign's std half (variadic `drop`, the formatter) needs phases 3-7 of the compiler.

**How to apply:**
- Merge the spp change, then push SPP-STL.
- Before pushing, test the working-tree std against the new compiler. Copy `../spp-stl` with `cp -a`, since rsync is not
  installed, into a scratch project's `vcs/std`, with `origin` pointed at the local repo so the pull is a no-op. Then run
  `spp test -l std`.
- After the push, `git -C tests/test_outputs/vcs/std checkout .` clears any hand-copied file, so the fixture can pull.

Related: [[spp-project-std-and-prelude]].
