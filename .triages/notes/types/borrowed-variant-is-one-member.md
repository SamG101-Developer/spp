---
name: borrowed-variant-is-one-member
description: "A borrow of a variant (&Opt[S32]) is one variant member, a pointer, never flattened; VariantMemberRefs still answers for it, so flattening code must check the borrow itself"
updated: 2026-11-04
---

A variant held by value is flattened into a variant containing it (`(S32 or Bool) or Str` is `S32 or Bool or Str`),
but a borrow of one (`&Opt[S32]`) is a single member: a pointer. `type_members::VariantMemberRefs` deliberately lists a
borrowed variant's members too (a `case` over a `&Opt` subject narrows through them), so every place that flattens or
compares members has to check the borrow itself: `AppendKeyId` (records members only when the variant is the whole
key, not after a convention), `FlattenVariants` (skips a borrowed member), `VariantAccepts` (a borrowed value is no
subset variant of an owned one), and on the identity side `VariantKey` and `RewriteTypeId` (`Conv == 0`).

**Why:** open-bugs #55 (2026-11-04). Three of those places missed it, so a generator's resume type `GenDone or
&Opt[S32]` (from `iter_ref` over `Vec[Opt[S32]]`) became `GenDone or None or Some[S32]`, and the yielded pointer's tag
was read as an address. It produced valid IR, so only a run-time check caught it (`check-coercions.sh`,
`iter-ref-over-variants`).

**How to apply:** new code that reads a variant's members to flatten, dedupe or test membership checks
`ref.IsBorrowed()` (or the key's convention) first. Related: [[variant-tag-order-is-address-order]].
