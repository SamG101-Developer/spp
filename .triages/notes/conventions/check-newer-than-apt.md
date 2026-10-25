---
name: check-newer-than-apt
description: "For any tool CI or setup takes from apt/brew/a runner image, check upstream for a newer release and pin it if newer"
updated: 2026-10-24
---

Whenever a tool comes from `apt` (or another distro or package manager default), check whether upstream has a later
release than the one that gets installed. If it does, pin the latest upstream version instead.

**Why:** ccache from apt on Ubuntu 24.04 was 4.9.x, which predates the 4.14.1 fix for C++20 module BMIs. The stale objects it
served broke the clang CI link ([[ccache-stale-clang-modules]]). The maintainer said (2026-10-24): "should always check if there is a later
version available compared to what is used from `apt`".

**How to apply:** when adding, debugging or reviewing any apt-installed dependency, look up the upstream latest
release (`gh release view --repo ...`) and compare. Pin through the existing pins manifest, not inline.
