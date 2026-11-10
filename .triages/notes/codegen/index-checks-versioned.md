---
name: index-checks-versioned
description: llvm_pass_bounds versions a loop by its index checks (one wide-width guard, a copy without them); proof by comparison only (S4.4)
updated: 2026-11-10
---

`VersionIndexChecksPass` (`../sources/spp/codegen/llvm_passes/llvm_pass_bounds.cpp`, at the vectoriser's start, before the AIR passes,
every target, O1 and up) takes an innermost loop with one real exit, a computable trip count, and index checks: exits
to a `sppc_check_failed` abort of kind 1 (`RuntimeCheck::Index`) on an induction variable rising by a constant against
a length the loop does not change (`i == len` with step 1, `i u>= len`, `i u> len`, either way round). One guard
before the loop tests each check's last value, `start + step * count` worked out at **twice the width** (at most
(2^w − 1) + (2^w − 1)², so it cannot wrap), against the length: true, a copy with the checks removed runs; false, the
original, which aborts at the same round with the same message. A guard SCEV can prove needs no copy. Loops over 400
instructions are left alone. The versioning itself is `VersionLoop` (`llvm_pass_loop_versioning`, shared with `llvm_pass_air`'s
exact sums), which also copies an abort block for the copy when its arguments read the loop's own values.

**Why:** an index check is an early exit that cannot be deferred, so it kept element-wise index loops scalar and
branchy; LLVM's IRCE does not apply, as IndVarSimplify has already rewritten our `i u< len` into `i == len` by the time
loops are rotated. checked_map 125 → 73 ms (2026-11-10).
**How to apply:** the guard must stay a proof by comparison (safety plan S4.4): never trust a no-wrap flag on the
induction variable, as it may come from the very checks being removed. A new check kind to hoist (slice ends) needs
its own recogniser and a case in `tests/codegen/check-bounds-hoisting.sh`, which checks that out-of-range loops still
abort at the first bad index with the unchanged message.
