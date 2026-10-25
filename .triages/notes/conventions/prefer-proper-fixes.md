---
name: prefer-proper-fixes
description: "The maintainer wants the proper, root-cause fix even when it is deeper or architectural; never ship a narrow band-aid (e.g. special-casing bin-ops) when the general mechanism is what's wrong"
---

Always favour the proper fix, even when it means a deeper or architectural change. Don't settle for a band-aid that
covers only the reported shape.

**Why:** 2026-09-28, comp-generic array sizes. I deferred only arithmetic sizes in templates and left `case` sizes
crashing. The maintainer: "go ahead with the proper fix, not just the bin-op bandaid. always favour the proper fix even if its a
deeper, potentially architectural change".

**How to apply:** when a fix only works for some shapes of the input, find the general mechanism that is wrong and fix
that instead. In that case it was evaluating in a template, plus `InferType` re-analysing the type every call.
- Offering the narrow version as an option is fine, but recommend the proper one and do it by default.
- Still run the usual verification (std, ./project, gtests), since deeper changes carry more risk.
