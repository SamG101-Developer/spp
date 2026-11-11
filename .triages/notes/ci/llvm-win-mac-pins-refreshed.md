---
name: llvm-win-mac-pins-refreshed
description: "[pin.llvm-win]/[pin.llvm-win-arm] are fixed release tags that refresh-pins.sh moves together; Linux floats on apt, macOS on brew llvm"
updated: 2026-11-09
---

Linux CI takes LLVM from apt.llvm.org with `[pin.llvm-lib] version = "23"`, so it gets the newest 23.x on every run.
Windows and macOS download a release tarball named by `[pin.llvm-win]` and `[pin.llvm-mac]` (tag, asset, sha256). Until
2026-11-07, `refresh-pins.sh` only checked the Windows pin and ignored the macOS one, so both stayed on
`llvmorg-23.1.0-rc3` while Linux and local builds were on 23.1.5. It now moves both to the newest final
`llvmorg-<major>.x.y`, where `<major>` comes from `[pin.llvm-lib]`. It moves them only when both assets publish a
digest. Final releases name their tarballs predictably (`clang+llvm-<v>-x86_64-pc-windows-msvc.tar.xz`,
`LLVM-<v>-macOS-ARM64.tar.xz`); release candidates do not (rc3 shipped `clang+llvm-23.0.0.3-*`), so pinning an rc is
done by hand.

Since 2026-11-09 macOS has no pin: it installs brew's `llvm` (or `llvm@<major>` once the unversioned formula moves
past `[pin.llvm-lib]`), because the release tarball is built without RTTI ([[macos-llvm-has-no-rtti]]).
`install-llvm-macos.sh` fails if brew's major differs from `[pin.llvm-lib]`.

**Why:** a fixed pin with no refresh drifts unnoticed. The Windows check reported "matches its pin" for weeks.

**How to apply:** after an LLVM point release, run `refresh-pins.sh` and confirm the Windows and macOS jobs pass. A new
major starts with `[pin.llvm-lib]`. `gh release view --json assets` does not show the digest field; use
`gh api repos/llvm/llvm-project/releases/tags/<tag>`.
