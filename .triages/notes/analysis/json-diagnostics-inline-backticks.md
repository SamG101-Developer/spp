---
name: json-diagnostics-inline-backticks
description: "JSON diagnostics turn INLINE_INFO/NOTE/HELP spans into backticks by matching their exact colour codes"
updated: 2026-11-06
---

`--message-format=json` keeps each `INLINE_INFO` / `INLINE_NOTE` / `INLINE_HELP` span in label messages, notes and help
as a backtick-quoted span ("Found type `S32`"), so an editor can render it as code. `MarkInline` in
`sources/spp/lsp/diagnostic.cpp` finds them by the macros' exact opening pair (bright yellow `ESC[93m` + italic `ESC[3m`)
and the `ESC[0m` reset that closes them. Code, title and `wrapped` are still plain `StripAnsi`.

**Why:** the colours are the only marker the macros leave, and the spp-idea plugin shows backticked spans as `<code>`.

**How to apply:** changing the macros' colours, or opening some other span with yellow + italic, breaks or fakes the
backticks. Update `kInlineOpen` / `kInlineClose` with them; `test_inline_highlights_are_kept_as_backticks` catches it.
