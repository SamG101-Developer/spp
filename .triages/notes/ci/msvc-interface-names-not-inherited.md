---
name: msvc-interface-names-not-inherited
description: "MSVC: a .cpp doesn't see its interface's use() using-declarations, and a template instantiated by another module loses them; use_alias in the .cpp, qualified names in templates"
updated: 2026-11-07
---

Since `use(ns, X)` became `using ns::X` (a55be4f4, 2026-11-07), MSVC (14.51) differs from GCC and clang in two ways:

1. **Implementation units.** A module's `.cpp` does not see the using-declarations its interface's `use(...)` lines make
   (GCC and clang do). Class names bound only that way gave "'OrderableAst': is not a class or namespace name" in 155
   files at once. Fixed by qualifying member definitions (`auto spp::asts::X::F(...)`) and giving each `.cpp` its own
   `use_alias(ns, X);` for every name its interface takes from a namespace that does not enclose the file's own.
2. **Templates.** A class template in an interface, instantiated by another module, has its non-dependent names
   looked up again from that module, which cannot see the interface's using-declarations: C2061 on the name, then
   C3668 "did not override". `InnerScopeAst::Stage11_CodeGen` and `comp_time_intrinsics.ixx`'s `CompTimeCallArgs`
   had been qualified for this and were unqualified by a55be4f4.

**Why:** both break only the Windows MSVC lane, usually once everything else compiles, so they surface one CI run at a
time.

**How to apply:** a new `.cpp` that uses a name from its interface's `use(...)` declares it again with `use_alias`
(add `module;` + `#include <spp/macros.hpp>` if the file has no fragment). In a class template, write names from other
namespaces qualified so they resolve from the template's own namespace (`asts::meta::CompTimeCallArgs` from
`spp::analyse::utils`), and keep such qualifications when tidying. Related: [[msvc-exported-inline-vars]].
