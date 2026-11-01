---
name: vcs-dependencies-one-level
description: "spp.compiler.dependencies is the one list of dependencies, flat under vcs/; a package names only what it declares (E121), std included; one namespace is one package"
updated: 2026-11-01
---

`spp.compiler.dependencies` is the only reader of `[vcs]`: `Declared` (one file's records, safe before a fetch),
`Fetch` (the walk, with the git work handed in by `HandleVcs`), `Resolve` (the fetched ones, every level) and `Validate`.
The last three share one walk, so they cannot disagree about the tree. The cli, `ModuleTree` and `ffi` all ask it.

The rules, each a hard error:

- Everything lands flat in `<root>/vcs/<name>`. One name declared with a different `git` or `branch` names both
  requesters. URLs compare as written.
- A module can only name its own package and what its `spp.toml` declares (E121, `visibility_utils::CheckPackageAccess`,
  beside ffi's E120). It is not a lookup rule: a value of an undeclared dependency's type can be held and used, and
  `meta.IsTestHarness` and `IgnoreAccessModifierViolations` are exempt.
- Every package declares `std` unless it is `std` (`Validate`), since the prelude names it. `std` can be pinned, but one
  pin has to hold across the whole project.
- A top-level namespace is one package (`ModuleTree::CheckNamespaces`, before parsing), and not both an ffi package and
  source. So `NamespaceSymbol::Owner` and `Declares`, which are per top-level namespace, are always the module's own.

See `.triages/vcs-dependency-model.md` for why, and what is out of scope (`ns`, lockfiles).

**Why:** the compiler used to have no such list. Each reader worked dependencies out from folders and paths, at different
depths, so removed dependencies still compiled, stray `.spp` files compiled, and a project under a folder named `vcs`
was misread as a dependency.

**How to apply:** never list `vcs/` or look for `vcs` in a path; ask `Resolve`, or read `Module::Owner`
(`ModulePrototypeAst::Owner` on the AST). Before a fetch, validate with `ValidateProject` only: `HandleValidate` also
requires every dependency to be fetched and sound, and a broken checkout must still be able to reach the fetch that
repairs it. Test the walk through `Fetch` with a fake `fetch_one` (`test_msc_dependencies.cpp`), and anything needing
real packages in `tests/codegen/check-transitive-deps.sh`, as the gtest fixture has only `std`.
