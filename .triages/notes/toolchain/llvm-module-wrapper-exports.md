---
name: llvm-module-wrapper-exports
description: "Extend the project's llvm module wrapper's exports freely rather than settling for a worse LLVM API"
updated: 2026-09-13
---

When an LLVM symbol isn't exported by the project's `llvm` C++ module wrapper, add it to the wrapper's exports instead of falling back to a weaker alternative.

**Why:** 2026-09-13, `llvm::PoisonValue` wasn't exported so I used `llvm::UndefValue` for dead `!` values; the user said poison is more applicable and to always feel free to edit the wrapper and export more.

**How to apply:** pick the right LLVM API first, then export it. Editing the wrapper means a near-full rebuild (everything imports `llvm`), so batch it with other header changes and don't edit it while a build is running. See [[build-workflow]].
