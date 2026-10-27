---
name: spp-project-std-and-prelude
description: "A project's vcs/std is a separate git clone that spp build re-fetches; prelude names must not be re-imported in project files"
updated: 2026-10-25
---

A project's `vcs/std` is its own git checkout of SPP-STL, not part of the project or of this repo. Edits to the
standard library show up in `git -C <project>/vcs/std diff`, never in the main diff, and need their own commit and
push. Every `spp build` runs a `git fetch` against it first, but local modifications survive.

Anything already in `kPrelude` (`headers/spp/compiler/prelude.ixx`) must NOT be re-imported in a project's
`src/*.spp` files: `use std::number::S32`, `use std::annotations::public`, `use std::void::Void` etc. all raise
`SppIdentifierDuplicateError`. Files inside the std library itself do import them, because the prelude is applied only
to the user project. `thread_hazard` is not in the prelude, so it does need importing.

**Why:** the error points at the `use` line rather than saying "this is a prelude name".

**How to apply:** in a scratch or user `.spp` file, import only non-prelude names. See [[spp-test-harness-stale-std]]
for the separate frozen clone under `tests/test_outputs/vcs`.
