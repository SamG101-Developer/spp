---
name: enums-over-strings
description: "Fields and params hold the enum, never its name; strings only where a user writes them, converted once at that edge"
updated: 2026-11-01
---

A field or parameter with a fixed set of values holds the enum, not a string naming it. `BuildMode` and `BuildType`
(`spp.compiler.out_layout`) are the worked example: `ModeFromName`/`TypeFromName` turn user text into the enum at the
edge (a CLI option, `spp.toml`, an env var), and `ModeName`/`TypeName` turn it back only where text is the point (the
`out/` folder, a `!cfg` comparison, the hardening record).

**Why:** the maintainer's rule, said 2026-11-01 while moving `OutLayout::Mode` off `Str`. A string field lets an invalid
value travel until something far away misbehaves, and every reader has to convert or compare spelled names.

**How to apply:** take the enum in the signature and convert once, at the outermost caller. A value that means "any of
them" is `Opt<Enum>` with `Nullopt`, not an extra enum member or a magic string, as `HandleClean` does for `--mode all`.
Locals that CLI11 binds to stay `Str`; it validates them with `IsMember` before the callback converts them.
