---
name: spp-case-narrowing-limits
description: "What a case subject narrows, and the narrowed symbol outliving its branch"
---

`CasePatternVariantDestructureObjectAst::Stage7_AnalyseSemantics` flow-types the subject when it is an
**`IdentifierAst`**, and (since 2026-09-06) when it is **`<name>@`** - a dereferenced borrow, which narrows the borrow
it dereferences. Only that shape is rewritten: a deref of anything else has no name to narrow, and reading the subject
without the `@` would evaluate a different expression rather than the same storage.

**The narrowed name is the plain name, and its type is the bare alternative** - no convention - whether or not the
subject was a borrow. So `case v@ of { is T(..) { v.method() } }`: write `v`, not `v@`. Inside the branch `v` is the
alternative's own value, so `v@` there is a deref of a non-borrow and is rejected. That is the same convention the
borrowed-subject form already used; both spellings now behave identically inside the branch.

Two things had to change together. The `_MappedLet` (`let <destructure> = <subject>`) is analysed *after* the flow
symbol is added, so it also takes the subject with the `@` stripped - left as `v@` it re-infers against the narrowed
`v` and fails to dereference it.

**What this bug looked like:** the branch was picked correctly (the tag comparison is generated either way) but the
body resolved against the variant, so a method call on the subject bound to the *variant's* own method. For a variant
method that dispatches to its alternatives that is unbounded recursion, which the optimiser deletes rather than
diagnoses, so the call quietly answered with the `else` value. `std::net::ip::IpAddr::family` is written
`case self@ of` and answered for no address at all; it is back to that spelling now, unmodified. Tests:
`regression::tst::codegen::test_a_case_on_a_{borrowed,dereferenced}_subject_narrows_for_dispatch`.

**The narrowed symbol shares the subject's name**, so it hides the subject for the rest of the branch - the point of
it, but it also means a lookup of that name *from inside the branch* answers with the narrowed one, which has no
storage of its own until later in the same routine. Stage11 therefore reads the subject's storage from
`_CondSymbol->ScopeDefinedIn`, not from the current scope. Getting that wrong crashed code generation whenever two `case`
blocks in one routine had subjects of the same name - two lines both calling theirs `ip` was enough. Note the crash
needs the `case`: plain shadowing (`let x = ..` twice, no `case`) has always worked. Test:
`regression::tst::codegen::test_two_case_blocks_over_a_shadowed_subject_name`.

**Still open, and unrelated:** `let x = x.unwrap()` - rebinding a name from its own value - reports "Target identifier
has no symbol", because the new symbol is registered before the right-hand side is analysed. Todo in
`identifier_ast.cpp`; no test, since it stops the whole compile rather than one test.

Related: [[spp-test-writing-idioms]], `net-module-findings` (SPP-STL notes).
