---
name: spp-toml-feature-schema
description: "spp.toml is schema-checked; memory protections are toml features, default on, queried via utils::features::Enabled"
---

`headers/spp/utils/features.ixx` + `sources/spp/utils/features.cpp` hold one table describing every section and key
`spp.toml` accepts, and the runtime answer to "is this protection on". Adding a protection means adding a `KeySpec`
to `Schema()` - there is deliberately no way to have a feature that is not documented there, and `spp config` prints
the table.

**Why:** an unrecognised key used to be ignored, so a typo'd `protct = false` would silently leave the protection on
while the author believed it off. `Validate` reports unknown sections/keys with the valid ones and a `ClosestMatch`
suggestion; unknown nested sections are reported as written (`[memry.stack]`, not `[memry]`).

**How to apply:** `utils::features::Enabled(ConfigKey::MemoryStackProtect)` - keys are named by the `ConfigKey` enum,
never by spelling a path, so renaming one in the schema is a compile error at every reader. Defaults live in the
schema and are on.
`Load` is called once per build in `HandleBuild` for the top-level project only - a dependency's own toml is
validated but its features are not adopted. Gate a protection at its *call site* when the implementation lives in a
plain (non-module) TU like `llvm_pipeline.cpp` (llvm_passes.cpp until the 2026-10-19 split), which cannot import the features module - that is why
`ApplyStackProtector` is gated in `compiler_boot.cpp` rather than inside itself. Checked arithmetic deliberately has *no* key: it is a
language-level guarantee, not a hardening measure, so `+` cannot be made to wrap by any configuration. Only the
things layered on top of the guarantees are switchable.
