---
name: add-tests-even-if-failing
description: "Add tests for known-missing compiler behaviour even when they fail, alongside a Todo at the code site"
updated: 2026-08-20
---

When work uncovers behaviour the compiler gets wrong but that is out of scope to fix now, add the test anyway (as
SHOULD_FAIL/SHOULD_PASS reflecting the *correct* behaviour) as well as a `Todo:` comment at the code site. Do not hold
the test back just because it will be red.

**Why:** The user asked for this directly (2026-08-20), after I deliberately withheld a red test for an escaping-borrow
gap. A red test is how they track what is still incomplete; a Todo comment alone is easy to lose.

**How to apply:** Only for tests that are red because of a genuine compiler bug — never for a test that is red because
it encodes uncertain or stale semantics. When the semantics themselves are in question, ask instead (see
[[spp-test-suite-stage-scope]] for what the suite actually runs).
