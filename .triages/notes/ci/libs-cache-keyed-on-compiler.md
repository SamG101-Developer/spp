---
name: libs-cache-keyed-on-compiler
description: "The small-libs cache key holds the compiler's banner hash; a runner label that changes compiler otherwise reuses module libs the old one built"
updated: 2026-11-25
---

The small-libraries tree (`~/.local`: ColEx, GenEx, NumEx, ...) is cached per lane, and `install-small-libs.sh` skips
a library whose stamp matches its manifest record. Until 2026-11-25 neither the key nor the stamp named the compiler
version, only the runner label. GitHub moved `windows-2025` to the VS 2026 image (MSVC 14.52), so the Windows MSVC
x64 lane kept linking a `ColEx.lib` the older MSVC built, rolled forward run to run through the restore-key, and
failed at the link: `lld-link: undefined symbol: ??Hcolex@@...AEBVFormat@$$_Acolex.format@@...` (colex's `operator+`
and `operator|`, with `Format` attached to its module). Every source compiled; only the prebuilt library was stale.

Now `.github/scripts/setup/compiler-id.sh` hashes `CXX`'s version banner (`cl` run bare; anything else
`--version`), and the libs key and its restore prefix carry it (`spp-libs-v3-<image>-<compiler><version>-<id>-...`),
so a tree is only ever restored onto the compiler that built it. The stamps need no compiler in them for that reason.

**Why:** C++ module libraries carry compiler-specific mangling and BMI expectations; the label is not the compiler.

**How to apply:** an undefined symbol from one of our module libraries, on one lane, with clean compiles, means a
stale prebuilt library: check the "Identify compiler" banner and the "already installed" lines, and delete that lane's
`spp-libs-*` caches (`gh cache list --key spp-libs-v3-<image>`, `gh cache delete <id>`). Any new cache of built
artefacts needs `steps.compiler-id.outputs.id` in its key too.
