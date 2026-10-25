---
name: cmake-stale-module-objects
description: "cmake/ninja silently skips rebuilding a changed C++ module TU; touch the .cpp when a change seems to have no effect"
updated: 2026-10-16
---

The ninja build sometimes does not rebuild a changed `.cpp` in this project's C++20 module setup, and the build
reports success while the binary still holds the old object. It has bitten twice, both times costing a full
build-and-run cycle to diagnose:

- Adding a virtual override (`FunctionImplementationAst::DiscardsFinalMember`) linked with
  `undefined symbol: ... DiscardsFinalMember() const` even though the definition was right there in the file.
- Registering a builtin in `sources/spp/analyse/utils/builtins.cpp` had no effect - the compiler still raised
  `compiler_builtin function 'std.mem.ops.drop' is not registered in kBuiltinFuncs`.

**How to apply:** when an edit appears to have had no effect - an undefined symbol for something you just defined, or
a registration/table entry the running binary does not see - do not go looking for a logic error first. `touch` the
`.cpp` (and any TU that consumes its interface) and rebuild. Both times that was the whole fix.

**An interrupted build leaves 0-byte objects** (2026-10-16: the session ended mid-build). Ninja then treats them as up
to date, and the link fails with `undefined symbol` for whole classes (every `GenericParameterAst` member) in files
nobody edited. Fix: `find <release build dir> -name '*.o' -size 0 -delete` (check `*.gcm` too), rebuild.

## `git stash` can silently de-sync a module interface from its implementation (2026-09-04)

Using `git stash push -- <files>` / `pop` to get a pristine baseline restored a **stale**
`sources/spp/asts/type_identifier_ast.cpp` while `headers/.../type_identifier_ast.ixx` kept the newer version. The
header declared a method the .cpp no longer defined; because the only call site was also in the reverted .cpp, it
**built and linked with zero errors** and five compiler behaviours silently stopped working.

`-Werror` will not catch this shape. After any stash/apply/checkout cycle, grep for a marker from every edit you made
before trusting the tree, and re-run the behavioural check - do not rely on "it built".
