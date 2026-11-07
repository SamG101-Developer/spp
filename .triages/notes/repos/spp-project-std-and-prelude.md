---
name: spp-project-std-and-prelude
description: "A project's vcs/std is a separate git clone; every module, std's own included, gets the prelude minus its own namespace's lines, so no file re-imports prelude names"
updated: 2026-11-06
---

A project's `vcs/std` is its own git checkout of SPP-STL, not part of the project or of this repo. Edits to the
standard library show up in `git -C <project>/vcs/std diff`, never in the main diff, and need their own commit and
push. Every `spp build` runs a `git fetch` against it first, but local modifications survive.

Every module gets the prelude (`kPrelude`, `sources/spp/compiler/prelude.cpp`), std's own sources too, through
`compiler::PreludeFor(NsParts)`. That drops the prelude lines importing from the module's own namespace, so
`std::string` gets everything but `use std::string::Str`. The same rule holds whether std is a `vcs` dependency or the
root project (building SPP-STL on its own, as the spp-idea plugin does). So no `.spp` file, in a project or in std,
may re-import a prelude name: `use std::number::S32`, `use std::annotations::public` and so on raise
`SppIdentifierDuplicateError`. `thread_hazard` is not in the prelude, so it does need importing.

Before 2026-11-06, std's sources skipped the prelude entirely (`Module::IsStd`) and imported those names themselves.
That broke any build with std as the root project, where its sources weren't flagged, with E14 in every std file.

**Why:** the error points at the `use` line rather than saying "this is a prelude name". A prelude name added to
`kPrelude` must also have any `use` of it removed from SPP-STL, or std stops compiling.

**How to apply:** in any `.spp` file, import only non-prelude names. See [[spp-test-harness-stale-std]]
for the separate frozen clone under `tests/test_outputs/vcs`.
