---
name: spp-mod-begin-extern-cpp-linkage
description: "A free function declared in a module whose .cpp uses SPP_MOD_BEGIN must be declared SPP_EXP_CLS, not SPP_EXP_FUN"
updated: 2026-09-14
---

`SPP_MOD_BEGIN` is `extern "C++" {` and `SPP_EXP_CLS` is `export extern "C++"` (headers/spp/macros.hpp). A .cpp
that wraps its definitions in `SPP_MOD_BEGIN ... SPP_MOD_END` (symbols.cpp, the AST .cpps) defines everything in the
global module. `SPP_EXP_FUN` is a plain `export`, which attaches the declaration to the named module, so pairing the
two fails:

`error: redeclaring 'uint64_t spp::analyse::scopes::NextGnParamId()' in global module conflicts with import`

**How to apply:** check whether the .cpp uses `SPP_MOD_BEGIN` before declaring a free function in its .ixx.
- If it does (e.g. symbols.cpp), declare the function `SPP_EXP_CLS`, even though the name says "class".
- If it doesn't (scope.cpp: `BumpScopeLinkageGeneration`, `TypeLookupGeneration`), use `SPP_EXP_FUN`.

Hit on 2026-09-14 (build 88) adding `NextGnParamId`. See [[gcc-module-forward-decl-chain]].

**Friends of an `SPP_EXP_CLS` class** (2026-10-07, `TypeKey` befriending `HeadOf` / `BareOf` /
`ArgsOf`): a friend declared inside the class is in its `extern "C++"` block, so it attaches to the global module. The
befriended function must therefore be `SPP_EXP_CLS` too, not `SPP_EXP_FUN`, and defined inside `SPP_MOD_BEGIN`.
Declare it ahead of the class, because a friend declaration first and an export later is ill-formed. Declare it there
*only*: a second declaration later trips `-Werror=redundant-decls`.

**Don't befriend an overloaded function.** Making `ParamsNamedBy(TypeId)` a friend (global module) while its
`ParamsNamedBy(CompKey)` overload stays module-attached in `comp_key` crashed GCC with an ICE: `lookup_mark`, at the
recursive unqualified call. It still crashed with *both* overloads moved to the global module (2026-10-07, tried), so
the trigger is a friend overloaded across modules, not mixed attachment. A non-exported `extern "C++" struct` accessor that the class befriends works around it (tried as
`TypeKeyCaches`), but the maintainer preferred the `TypeKey::_Cached*` fields simply public with the `_` prefix (2026-10-07).


**Qualify calls to exported free functions from member bodies.** An unqualified `PrecompiledTemplate(tmpl, scope)`
inside `TypeRef::IsA` (symbols.cpp) hit the same `lookup_mark` ICE (2026-10-08); `spp::analyse::scopes::PrecompiledTemplate`
compiles. If a tidy drops a qualification and the build ICEs in `lookup_mark`, restore it.
