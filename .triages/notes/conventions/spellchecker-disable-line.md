---
name: spellchecker-disable-line
description: Deliberate typos (error-message tests) take a trailing "spellchecker:disable-line" marker; typos and codespell skip the line
updated: 2026-10-26
---

A line containing a deliberate misspelling ends in `spellchecker:disable-line`, in whatever comment syntax the line
has. That's `# spellchecker:disable-line` inside an S++ test program, which the compiler reads as a comment. Both
pre-commit spell hooks skip such a line, through `pyproject.toml`: typos `extend-ignore-re` and codespell
`ignore-regex`. The first user is `TestErrorMessages.test_unknown_attribute_suggests_through_forwarding`.

**Why:** suggestion tests ("did you mean 'value'?") need real typos. The other options were allowing the word
project-wide, excluding the whole file, or hunting for a misspelling no dictionary knows, and they either hide real
typos or break on a dictionary update.

**How to apply:** mark only the line with the deliberate typo. Don't add the word to `extend-words` or
`ignore-words-list`, which are for real identifiers (`olt`, `compex`).
