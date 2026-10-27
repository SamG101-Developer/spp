---
name: spp-test-group-filter-bugs
description: "`spp test` quirks: a novel !test group name segfaults every test in it, and --group filtering makes some groups fail that pass in a full run"
---

Two independent bugs in the `spp test` harness, both of which look like the code under test is broken:

1. A `!test(group="...")` naming a group **no existing test already uses** makes every test in that group segfault -
   `std::debug::assert(1 == 1, ...)` included. The same tests in the same new file pass under an existing group name,
   so it is the group rather than the file. Workaround: reuse a neighbouring group and leave a Todo.
2. `--group <name>` changes results. `std/threading/atomic` is 0/12 under the filter and passes in a full
   `spp test --lib std` run. Trust the full run; a filtered run is not a baseline. Probably `cmp`/global
   initialisation tied to which modules get built - see [[spp-codegen-cross-module-and-inherited]].

**Why:** (1) sent me hunting a nonexistent `Mutex` bug for several build cycles, and (2) made me wrongly report
atomics as a rel-profile intrinsics regression.

**How to apply:** when adding s++ unit tests, reuse an existing group name, and confirm any failure with a full
`spp test --lib std` (509/511 as of 2026-09-06; the two reds are the known `Path::components` and `RawBuf` gaps in
`std-known-gaps` (SPP-STL notes)).
