---
name: heaptrack-needs-mimalloc-removed
description: "heaptrack sees almost nothing in spp builds: mimalloc's operator new bypasses malloc; build with -DSPP_NO_MIMALLOC and patchelf out libmimalloc.so.2"
updated: 2026-10-16
---

heaptrack (installed 2026-10-16) only hooks `malloc`/`free`. In a normal release build the mimalloc wrapper defines
`operator new` as `mi_new`, and even with that compiled out `libmimalloc.so.2` is a DT_NEEDED library that exports its
own `operator new` ahead of libstdc++'s. Symptom: ~1,500 allocation calls and a few hundred KB of heap for a whole
compile, all inside libc.

**Recipe (used for N1.13):**
- Profiling build: `cmake -S . -B cmake-build-release-heap -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_CXX_FLAGS="-DSPP_NO_MIMALLOC -g1"` (same compilers and SPP_* options as the main build; gitignored).
- Copy `tests/spp_tests` from it and `patchelf --remove-needed libmimalloc.so.2` the copy (no `mi_` symbols are
  imported, so it is safe).
- `heaptrack -o out <copy> --gtest_filter=...`, then `heaptrack_print -f out.zst --flamegraph-cost-type peak -F
  stacks.txt` and diff inclusive cost per function between two runs (1 vs N compiles) to see what grows.

Run the test binary from a directory where `../../tests/test_outputs` resolves (the scratchpad `n112` dir worked), or
every test fails in 0 ms. Related: `ask-to-install-missing-tools`, [[spp-test-suite-stage-scope]].
