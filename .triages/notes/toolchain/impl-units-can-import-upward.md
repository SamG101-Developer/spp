---
name: impl-units-can-import-upward
description: "A module's .cpp implementation unit can import modules above it (scopes .cpp → utils) with no cycle; only interface (.ixx) imports form cycles, so function-pointer \"install at boot\" hooks are unneeded"
---

`CompMembers::Find` was a function pointer that `CompilerBoot` set to `comp_generics::FindCompMemberId`, read
through `scopes::CompMemberIdOf`. It existed so `type_key` could read a constant without importing `utils`. It was
never needed: `type_key.cpp` is an implementation unit, nothing imports it, and `comp_generics.ixx` only imports
`type_key.ixx`, so there's no cycle. Removed 2026-10-07; all callers now call `comp_generics::FindCompMemberId`.

**Why:** `scope.cpp`, `scope_manager.cpp`, `symbol_table.cpp`, `symbols.cpp` and `type_key.cpp` all import `utils`
modules already. Only interface-to-interface imports can form a module cycle.

**How to apply:** when a lower module's code needs something from a higher module, import it in the `.cpp`. Only
reach for an injected hook if the *declaration* (the `.ixx`) needs it. The GCC bug in
[[gcc-module-forward-decl-chain]] is about interface imports and forward declarations, not this.
