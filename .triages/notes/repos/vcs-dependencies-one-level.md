---
name: vcs-dependencies-one-level
description: "vcs dependencies are one level deep throughout: a dependency's own [vcs] is never fetched, and a nested vcs folder is half-seen"
updated: 2026-11-01
---

Only the project's own `[vcs]` table is fetched (`cli::HandleVcs`), and each dependency's `spp.toml` is not read for
more. Everything else assumes a dependency is a direct child of `vcs/`: `ffi.cpp`'s `FfiFolders`, `ModuleTree`'s
`_SourceRoots`, `OwnerOf`, and `PackageOfScope` in `sup_prototype_extension_ast.cpp`. So a project has to list every
transitive dependency itself.

**Why:** a `vcs/a/vcs/b` that exists on disk is not ignored. `GlobSpp(_VcsPath)` is recursive, so its modules are read,
but the `main.spp`, `tst` and `ffi` filters only compare against the top of `vcs/a`. Its `main.spp`, tests and ffi stubs
are therefore kept as ordinary modules, under no source root, namespaced by their parent folder, and its libraries are
never linked. Read from the code on 2026-11-01, not run.

**How to apply:** do not make `FfiFolders` alone recursive. Support transitive dependencies by having `HandleVcs` walk
each dependency's `[vcs]` and flatten into one `vcs/` folder, deciding what a name clash means: `ffi::Validate` already
rejects the same ffi package name twice.
