---
name: syntax-rewrites-from-a-parser-dump
description: "Rewrite a syntax change with a temporary parser dump of token offsets, not a regex; where the s++ snippets hide and the offset traps"
updated: 2026-11-16
---

For a syntax change a regex cannot place (the closure `(...)` to `|...|` change of 0.7.0: `()` looks like a tuple), let
the current parser find the sites. Temporarily: record the token indices in the parse function, keep them in a
registry keyed by the AST pointer, and erase the entry in the AST's destructor, so the ASTs that backtracking throws
away drop out and only the final tree's remain. A throwaway gtest lexes and parses each listed file and prints byte
offsets; a script swaps exactly those bytes, checking they hold the expected characters first.

**Offset traps:** a token carries no source offset. `Lexer` prepends a `"\n"` to the code, comments produce no tokens,
and a CR is dropped. So align each token's `Data` against `"\n" + code` (skipping `#` to the end of the line), then
subtract one. **GCC:** an exported `inline` variable in a module interface fails ("odr-used inline variable is not
defined"); export a function under `SPP_EXP_CLS` (`export extern "C++"`), defined in the `.cpp`.

**Where the s++ is:** `.spp` files (std `src`/`tst`, `project`, `examples`, `tests/differential`, `benchmarks`,
spp-stl `.triages` probes); gtest `R"(...)"` raw strings; codegen checks' heredocs **and** single-quoted shell strings
(`fn_case "name" '...'`), plus a few double-quoted ones (`check-ffi-unwind.sh`) found only by grep. Snippets that do not
parse (syntax-error tests, C/TOML heredocs) give no sites: grep those by hand.

**Why:** the closure change hit ~360 sites in ~80 files with no false hits; a regex would have caught tuples.
**How to apply:** for the next syntax change, rebuild this from this note; check by a rough regex count per file
against the dump's count, and read every file where they differ.
