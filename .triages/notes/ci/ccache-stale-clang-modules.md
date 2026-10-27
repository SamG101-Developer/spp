---
name: ccache-stale-clang-modules
description: "ccache before 4.14.1 does not hash -fmodule-file BMIs, so clang importers get stale objects; fixed upstream (#1774); GCC module compiles uncacheable"
updated: 2026-10-24
---

ccache before 4.14.1 hashes a clang TU's preprocessed text, which holds `import m;` but not the BMI. When a module interface changes
and the importer's .cpp does not, ccache returns the old object. Proven 2026-10-24 with a two-file probe: after
`f(int)` became `f(int, int = 0)`, the importer was a cache hit that still referenced `_ZW1m1fi`. For GCC `-fmodules`,
ccache counts every compile as "Uncacheable", so the result is correct but nothing is cached.

It showed up in CI on linux clang as `undefined symbol: marker_sups::BuildFwdCall(...)` with the old 4-arg mangling,
from overload_resolution.cpp.o, after 60c2f9cc added a defaulted parameter.

**Why:** the clang CI builds were cached through ccache, first by a hand-written actions/cache step and then by ccache-action
from 2026-10-21 ([[ccache-action-key-quirks]]). A stale object shows up only once an interface changes and its importer doesn't.

**Removed then reinstated 2026-10-24:** the maintainer committed the removal (9dda4463), then had it put back with a pinned release.
setup-toolchain's "Install compiler cache" step runs install-compiler-cache.sh, which installs ccache from [pin.ccache]
(4.14.2: musl-static on Linux, darwin on macOS) and sccache from [pin.sccache] on Windows, then ccache-action runs with
`install: "no"`. refresh-pins.sh bumps both. See [[check-newer-than-apt]].

**How to apply:** never cache C++ module compiles with a ccache older than 4.14.1 (apt's included). If an undefined symbol
in CI mangles an older signature, suspect a stale object in a restored build tree or a compiler cache.

**Fixed upstream:** ccache#1774 (ff65435cdb) hashes `-fmodule-file=` contents from 4.14.1. The 4.14.2 glibc and musl
binaries passed the probe on 2026-10-24: plain flag, CMake's `@modmap` response file, and no-direct mode all missed on an
interface change, and an unchanged BMI hit. GCC (both tools) and cl.exe (sccache) module compiles are refused, which is correct but uncached. They are tracked as
U11/U12 in .triages/upstream-watch.md. sccache 0.19.0 caches clang-cl modules (#2876); that is untested here.
