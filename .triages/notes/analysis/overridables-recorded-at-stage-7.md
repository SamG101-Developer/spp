---
name: overridables-recorded-at-stage-7
description: "The editor's 'what can this sup ext block still override' record is made in Stage 7, not CheckExtensionMembers"
updated: 2026-11-06
---

`resolution_index::RecordOverridables` (JSON `"kind":"overridables"`) lists, per `sup X ext Y` block in an indexed file,
Y's abstract/virtual methods the block does not override yet, with the header to write. It is called from
`SupPrototypeExtensionAst::Stage7_AnalyseSemantics` once the superclass is analysed, not from `CheckExtensionMembers`
where the override check lives. The block's types are emitted as written (`WrittenText()`), because `Name->ToString()`
is already qualified by then (`main::Circle`).

**Why:** it reads `overload_resolution::MethodsOf`, which caches per scope and name; asked before stage 6, the cached
table could be incomplete for every later caller. `CheckExtensionMembers` also raises part-way through a bad block.
The spp-idea plugin matches a block to its record by the written names.

**How to apply:** keep the call at or after stage 7, and keep the names as written.
`test_the_index_lists_what_an_extension_can_still_override` covers it.
