---
name: std-tests-cannot-see-unreachable
description: "spp test builds rel+LTO: a std !test reaching an abstract method's `unreachable` passes; and an earlier direct call can mask a wrong instantiation"
updated: 2026-10-14
---

A std `!test` cannot catch a call that wrongly reaches an abstract method. An abstract body lowers to `unreachable`,
and `spp test` builds in **rel** with LTO, which folds the path away together with the assert, so the test passes
over a real miscompile (found 2026-10-15, N3.15). A dev-mode probe of the same code returned 0 instead.

Second trap from the same day: a direct call of an inherited default, compiled before the faulty (forwarded) call,
left an instantiation the faulty call reused, so it got the right answer by accident.

**Why:** both made a red regression test pass while the bug was present; only the IR showed it.

**How to apply:** for a "reaches the wrong method" regression test, give the base a defined body (`!virtual_method`
returning a sentinel), not `!abstract_method`; make only the call under test, with no earlier call of the same
method in the module. Confirm red before the fix, and read `out/.../rel/llvm/**.ll` if a red test is green.
See [[spp-probe-harness]], [[rel-profile-miscompiles-with-intrinsics]].
