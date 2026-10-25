---
name: msvc-transitive-namespace-lookup
description: "MSVC 14.51 sees namespaces from a plain (non-export) transitive import; relative `utils::` inside spp::analyse binds to spp::analyse::utils"
updated: 2026-10-21
---

MSVC 14.51 (VS 18, windows-2025 runner, Oct 2026) makes a namespace visible through a plain `import` in an imported module. GCC does not. Example: semantic_error_builder.ixx imports scope.ixx, which plainly imports memory_state (`spp::analyse::utils`). So `utils::errors::X` written inside `spp::analyse::errors` binds to `spp::analyse::utils` and fails with C3083/C2039. Fixed 2026-10-21 by writing `spp::utils::errors::` in full.

**Why:** Linux CI can't catch it; it shows up only on the Windows worker.
**How to apply:** inside `spp::analyse::*` or `spp::codegen::*`, write `spp::utils::` in full, never a relative `utils::`. Related: [[msvc-requires-clause-token-match]].
