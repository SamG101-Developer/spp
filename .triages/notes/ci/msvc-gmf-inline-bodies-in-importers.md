---
name: msvc-gmf-inline-bodies-in-importers
description: "MSVC \\\"uses undefined class basic_istream::sentry\\\" in a third-party inline function means the importer is instantiating GMF bodies; precompile them in a plain TU"
updated: 2026-09-11
---

Windows CI (windows-2025, VS 18 / MSVC 14.51.36231, first seen 2026-09-11) failed compiling `cli.cpp` with
`xstring(3537): error C2079: '_Ok' uses undefined class 'std::basic_istream<...>::sentry'`, from CLI11's
`StringTools_inl.hpp:666` (`iss >> word` in `streamOutAsParagraph`). Nothing in spp or the CLI11 pin changed: the
runner toolset did. CLI11's `inline` bodies sit in the `cli11.ixx` GMF, MSVC instantiates them in each *importer*,
and there the GMF's std entities are not all reachable.

**Fix applied:** `#define CLI11_COMPILE` before the include in `headers/wrappers/cli11.ixx` (declarations only), plus
`sources/wrappers/cli11.cpp`, a plain TU including every `CLI/impl/*_inl.hpp`. Not verified on MSVC locally; CLI11
*templates* still instantiate in the importer, so an ostream-sentry error from one of those would be the same bug.

The two halves must ship together: 858e42eb committed the ixx define but left `sources/wrappers/cli11.cpp`
untracked, so Ubuntu CI failed to link `spp` with ~20 undefined `CLI::App`/`CLI::Option` symbols while local builds
passed (the glob found the untracked file). Check `git status` for untracked new TUs before calling a build fix done.

**How to apply:** the same symptom from another wrapper (tomlpp, etc.) gets the same treatment when the library
has a compiled mode. Do not reach for a GMF `#include <istream>` - see [[import-std-not-stl-headers]]. Related:
[[msvc-strong-ordering-import-std]].
