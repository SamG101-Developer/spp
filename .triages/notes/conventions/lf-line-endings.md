---
name: lf-line-endings
description: "Files I change must be LF, never CRLF (CRLF breaks the maintainer's git hooks); check with git ls-files --eol after editing"
updated: 2026-09-28
---

Every file I create or modify in this repo must use LF line endings, never CRLF.

**Why:** CRLF working-tree files break the maintainer's git hooks (2026-09-28). The repo index is all LF. The maintainer's global
`~/.gitconfig` has `core.autocrlf=true`, which is where the "LF will be replaced by CRLF" warnings come from; leave that
setting to the maintainer. In one session 81 files I had touched ended up fully CRLF while every untouched file was LF. The source
was never pinned down: Python rewrites write LF, so suspect the edit tools or the IDE.

**How to apply:** after a batch of edits, and before handing work back, run:
- `git ls-files --eol | grep w/crlf` to list working-tree CRLF files.
- `sed -i 's/\r$//' <file>` on the ones I changed.

Don't touch CRLF files I didn't change (e.g. a file the maintainer staged), and never commit CRLF.
Related: `show-diff-before-scripted-edits`.

**Update (same day):** files converted to CRLF again *between* my edits, minutes after I wrote them LF: the 81 files
flipped back all at once, and one file I had just written flipped too. So something outside my edits rewrites them.
Most likely it's git checkouts under `core.autocrlf=true` (a stash/checkout/hook), or the IDE. So, before every edit:
- Normalise the file first (`sed -i 's/\r$//'`).
- Write with LF (`open(p, 'w', newline='')`).
- Re-check `git ls-files --eol` at the end of a task.
I flagged the autocrlf setting to the maintainer as the probable cause.
