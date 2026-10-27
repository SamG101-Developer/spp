---
name: lazy-attach-clean-context
description: "The lazy sup attach (OnSupScopesRead) runs mid-analysis; it must start from CompilerMetaData::ResetContext or it inherits e.g. TypeAnalysisTypeScope"
---

`Scope::OnSupScopesRead` attaches a scope's sups on first read, which can be inside any analysis: keying a type, or a lookup inside a qualified name. It used to inherit that analysis's meta. `std::option::Opt[Bool]` left `TypeAnalysisTypeScope` set, and `$IsSome` then failed E26 in `option.spp` (2026-09-29, it broke ./project).

**Why:** the attach is not part of the reading analysis.

**How to apply:** anything run lazily from a lookup hook gets `MetaGuard(meta, true)` plus `meta->ResetContext()`. `ResetContext` keeps the stage, `CompTimeResult` and the LLVM generator. Related: [[sup-attach-fixed-point]].
