---
name: sibling-libs-install-release
description: "spp links NumEx's compiled arithmetic from the install; a build-type-less install is -O0 and cost about a third of compile time"
updated: 2026-11-01
---

spp links NumEx, GenEx and ColEx from `/usr/local`, built by their own `CMakeLists.txt`, so spp's `-O3` does not reach
what is compiled into their `.a`. NumEx's arithmetic all lives there. GenEx is templates, instantiated in spp at spp's
flags, and ColEx only formats colours, so neither shows up in a profile.

**Why:** found 2026-11-01. All three had been installed from a `build/` with an empty `CMAKE_BUILD_TYPE`, which is `-O0`
and gives no warning. With NumEx's own rewrite on top, `./project` went from 4.39 s to 2.92 s. All three now default to
`Release` when no type is given, but a `*Targets-noconfig.cmake` in `/usr/local/lib/cmake/<name>/` marks an old install.

**How to apply:** before reading NumEx frames in a profile as slow code, check how it was built: tiny helpers such as
`IsNegative` and `std::views::zip` internals appearing as frames of their own mean `-O0`, and `objdump -d` on the
archive's `big_int.cpp.o` shows a stack frame in `IsNegative` if so. Related: [[spp-perf-hot-path]].
