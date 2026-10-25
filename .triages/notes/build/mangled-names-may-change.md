---
name: mangled-names-may-change
description: "Refactors may change mangled names; only uniqueness must hold - verify defines stay one-to-one, not byte-identical IR"
updated: 2026-10-15
---

The maintainer does not need mangled names to stay stable across refactors (2026-10-15, N6.16): an upgrade may rename symbols
freely, as long as every name that was unique before stays unique.

**Why:** the names carry no compatibility promise; requiring byte-identical IR blocked defining the instance-argument
order properly (N6.13 phase 2 found the order was an accident of symbol-table insertion).

**How to apply:** when an IR diff differs only in names, check per module that the number of `define`s and of distinct
defined names is unchanged (and equal to each other), and that bodies match once names are normalised - then the change
is fine. Report the renames, don't revert them. Bodies or counts differing is still a real change. See
[[spp-ir-diff-verification]].
