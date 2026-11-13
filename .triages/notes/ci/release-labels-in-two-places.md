---
name: release-labels-in-two-places
description: A release platform's label lives in CMake (SPP_RELEASE_LABEL, what self-update downloads) and post_merge.yaml (what is published); keep them equal
updated: 2026-11-13
---

Each release platform has a label: `linux-x64`, `linux-arm64`, `macos-arm64`, `windows-x64`. `post_merge.yaml`'s
`collect-release` names the zips by it (`collect.sh add <label>`, giving `spp-<label>.zip`), and `CMakeLists.txt` maps
the build's system name and processor to the same label as `SPP_RELEASE_LABEL` (in the generated `spp/version.hpp`),
which `spp self update` (`sources/spp/cli_self.cpp`) downloads. A platform CMake maps to no label is refused by the
update ("no release of spp is built for this platform").

**Why:** nothing checks the two lists agree. A label CMake gives that no release publishes makes every update of that
build fail on a missing `spp-<label>.zip`; a published platform CMake does not map leaves its users unable to update.

**How to apply:** adding or renaming a release platform changes both: the `collect.sh add` step and the CMake mapping.
`check-self.sh` derives its label from `uname` the same way, for the Linux and macOS ones. Related:
[[third-party-notices]].
